"""Local, hardware-free acceptance gate for a candidate SDK checkout."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from tools.nsx_npu_compile_check import find_gcc
from tools.test_evidence import phase_status, completion

import pytest


@dataclass(frozen=True)
class Requirement:
    selector: str
    minimum: int = 1
    boards: tuple[str, ...] = ()
    parameters: tuple[str, ...] = ()
    variants: tuple[tuple[str, ...], ...] = ()
    compiler: str | None = None


PAYLOAD = tuple(Requirement(f'tests/{name}.py') for name in (
    'test_artifact_baseline', 'test_committed_intake_patches',
    'test_intake_patches_byte_for_byte', 'test_sdk_intake', 'test_intake_workflow',
    'test_sdk_intake_review',
    'test_cmake_contract', 'test_manifests'))
# Minimum counts deliberately describe the reviewed matrix, independently of
# the test parametrization. Removing coverage must not silently weaken the gate.
GCC = (
    Requirement('tests/test_vendor_abi.py::test_vendor_layout_from_sdk_target', 6,
                parameters=('language', 'selection'),
                variants=tuple((language, selection) for language in ('C', 'CXX')
                               for selection in ('default', 'atfe', 'conflicting-global')), compiler='gcc'),
    Requirement('tests/test_link_smoke.py::test_smoke_is_linked_firmware', 4,
                ('apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
    Requirement('tests/test_audio_buffers.py::test_audio_real_headers', 6,
                ('apollo3_evb', 'apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
    Requirement('tests/test_power_core_clock.py::test_performance_compiles_with_real_hal', 8,
                ('apollo3_evb', 'apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo4l_evb', 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
)
# Acceptance matrices are deliberately independent of test parametrization so
# replacing a scenario in the tests cannot silently replace the requirement.
_AUDIO_BUFFER_SCENARIOS = (
    'valid', 'hfrc2-low', 'hfrc2-high', 'stereo', '24bit', 'packing',
    'pcm16-boundaries', 'pcm24-boundaries', 'overflow', 'channel-overflow',
    'zero', 'frame-align', 'channels', 'width', 'unknown-width', 'high-address',
    'dma-short', 'pcm-short', 'dma-align', 'pcm-align', 'address-wrap',
    'instance', 'count-too-large', 'count-limit',
)
_AUDIO_LIFECYCLE_COMMON = (
    *(('init', fault) for fault in ('initialize', 'power-on', 'configure', 'flush',
                                   'threshold', 'irq-clear', 'irq-enable')),
    *(('start', fault) for fault in ('enable', 'dma-start')),
    *(('stop', fault) for fault in ('irq-disable', 'irq-clear', 'disable', 'power-off', 'deinit')),
    ('isr', 'service'),
    *((phase, 'none') for phase in ('ownership', 'callback-stop', 'callback-restart')),
    *(('init-cleanup', fault) for fault in ('power-on/power-off', 'configure/power-off', 'configure/deinit')),
    *(('hardware-error', bit + suffix) for bit in ('derr', 'undfl', 'ovf') for suffix in ('', '+dcmp')),
)
_AUDIO_LIFECYCLE_APOLLO5 = (
    ('init', 'clock'), ('stop', 'dma-stop'), ('isr', 'status'), ('isr', 'irq-clear'),
    *((phase, 'none') for phase in ('ping-pong', 'bad-buffer', 'threshold-error')),
    *(('clock-policy', fault) for fault in ('init-hp', 'start-hp', 'init-query', 'start-query', 'hfrc-hp')),
)
# Host fault models exercise NSX adapters without microphones or probe access.
# Counts and backend requirements are acceptance criteria, not discovered from tests.
ADAPTER_REGRESSIONS = (
    Requirement('tests/test_timer_period.py::test_timer_period', 180),
    Requirement('tests/test_timer_period.py::test_timer_nonperiodic_and_defaults', 12),
    Requirement('tests/test_timer_period.py::test_timer_smoke_builds', 4,
                ('apollo4p_blue_kxr_evb', 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb')),
    Requirement('tests/test_timer_period.py::test_timer_other_variants_compile', 2,
                ('apollo4l_evb', 'apollo510dL_evb')),
    Requirement('tests/test_ap3_burst.py::test_ap3_performance_contract', 200),
    Requirement('tests/test_ap3_burst.py::test_ap3_performance_builds', 2,
                ('apollo3_evb', 'apollo3p_evb')),
    Requirement('tests/test_power_selectors.py::test_family_performance_contract', 20,
                parameters=('selector_binary', 'scenario'),
                variants=tuple((family, scenario) for family in ('apollo4', 'apollo4-fpga')
                    for scenario in (('low', 'high', 'max', 'repeat', 'invalid', 'invalid-after-init',
                                      'enable-fails', 'disable-fails') +
                                     (('wrong-hardware', 'missing-ack') if family.startswith('apollo4') else
                                      ('init-fails', 'init-no-output', 'unavailable'))))),
    Requirement('tests/test_audio_buffers.py::test_audio_buffer_bounds', 48,
                parameters=('audio_binary', 'scenario'),
                variants=tuple((backend, scenario) for backend in ('apollo3', 'apollo5')
                               for scenario in _AUDIO_BUFFER_SCENARIOS)),
    Requirement('tests/test_audio_buffers.py::test_audio_dma_cache_views', 2,
                parameters=('scenario',), variants=(('pcm16-boundaries',), ('pcm24-boundaries',))),
    Requirement('tests/test_audio_lifecycle.py::test_audio_lifecycle_failure', 66,
                parameters=('lifecycle_binary', 'phase', 'fault'),
                variants=tuple((backend, phase, fault) for backend in ('apollo3', 'apollo5')
                               for phase, fault in _AUDIO_LIFECYCLE_COMMON)
                         + tuple(('apollo5', phase, fault) for phase, fault in _AUDIO_LIFECYCLE_APOLLO5)),
    Requirement('tests/test_power_core_clock.py::test_performance_updates_core_clock', 133,
                parameters=('clock_harness', 'scenario'),
                variants=tuple((target, scenario)
                    for target in ('apollo510', 'apollo510b', 'apollo330P', 'apollo510L',
                                   'apollo510-fpga', 'apollo330P-fpga', 'apollo510L-fpga')
                    for scenario in ('low', 'high', 'max', 'not-set', 'select-fails', 'query-fails',
                                     'zero-source', 'excessive-source', 'low-source', 'audio-high',
                                     'audio-max', 'adjusted-250', 'unsupported-source', 'audio-then-free',
                                     'pending-audio', 'released-audio', 'invalid', 'missing-ack', 'wrong-hardware'))),
    Requirement('tests/test_power_core_clock.py::test_performance_smoke_builds', 4,
                ('apollo4p_blue_kxr_evb', 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb')),
)
CORE_BUILDS = (
    Requirement('tests/test_power_link.py::test_power_module_links_real_sdk', 8,
                ('apollo3_evb', 'apollo3p_evb', 'apollo4p_evb',
                 'apollo4p_blue_kbr_evb', 'apollo4p_blue_kxr_evb',
                 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb')),
    Requirement('tests/test_runtime_suite.py::test_runtime_links_real_handlers_and_heap', 6,
                ('apollo3p_evb', 'apollo4p_blue_kbr_evb', 'apollo4p_blue_kxr_evb',
                 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb')),
    Requirement('tests/test_runtime_suite.py::test_startup_links_real_sdk', 3,
                ('apollo3p_evb', 'apollo4p_blue_kbr_evb', 'apollo4p_blue_kxr_evb')),
    Requirement('tests/test_cpp_startup.py::test_cpp_constructor_image', 6,
                parameters=('soc', 'crt_overrides'),
                variants=tuple((soc, override) for soc in ('apollo2', 'apollo3', 'apollo3p')
                               for override in (False, True)), compiler='gcc'),
)
POWER_STARTUP = (
    Requirement('tests/test_apollo5_power_errors.py::test_apollo5_first_error', 80,
                parameters=('power_errors', 'keep', 'mask', 'small', 'ssram'),
                variants=tuple(((family, validation), keep, mask, small, ssram)
                    for family in ('apollo510', 'apollo510b', 'apollo5b', 'apollo330p', 'apollo510l')
                    for validation in (True, False) for keep in (0, 1) for mask in (0, 1)
                    for small, ssram in ((0, 1), (1, 0)))),
    Requirement('tests/test_apollo5_power_errors.py::test_power_startup_smoke_builds', 3,
                ('apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb')),
    Requirement('tests/test_apollo5_power_link.py::test_power_module_links', 4,
                parameters=('family', 'board'),
                variants=tuple(('gcc', board) for board in
                    ('apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb', 'apollo510dL_evb')), compiler='gcc'),
    Requirement('tests/test_ap510l_power.py::test_ap510l_backend_policy', 48,
                parameters=('ap510l_power', 'small', 'ssram', 'keep', 'failure'),
                variants=tuple((validation, small, ssram, keep, failure)
                    for validation in (True, False) for small in (0, 1)
                    for ssram in (0, 1) for keep in (0, 1) for failure in (0, 1, 2))),
    Requirement('tests/test_debug_state.py::test_debug_transport_contract', 12,
                parameters=('scenario',), variants=tuple((s,) for s in
                    ('system', 'resume', 'switch', 'noop', 'setup-failure', 'switch-failure',
                     'disable-failure', 'hardware-reset', 'system-reapply', 'hardware-release-failure', 'power-release', 'power-release-failure'))),
    Requirement('tests/test_usb_connection.py::test_usb_connection_contract', 20,
                parameters=('connection_binary', 'scenario'),
                variants=tuple((owner, scenario) for owner in ('nsx-unmount', 'dcd-unmount')
                               for scenario in ('invalid', 'cdc-dtr', 'vendor-lifecycle',
                                                'queued-unplug', 'queued-suspend', 'queued-open',
                                                'cdc-buffers', 'vendor-buffers', 'receive-status', 'reconnect'))),
    Requirement('tests/test_usb_cdc_send.py::test_cdc_send_fifo_progress', 18,
                parameters=('cdc_send_binary', 'scenario'),
                variants=tuple((family, scenario) for family in ('ordinary', 'apollo5-flush')
                               for scenario in ('fit', 'boundary', 'large', 'wide-length',
                                                'initial-full', 'short-write', 'partial',
                                                'stalled', 'default-timeout'))),
    Requirement('tests/test_usb_reentrancy.py::test_usb_reentrancy', 14,
                parameters=('transport', 'scenario'),
                variants=tuple((transport, scenario) for transport in ('cdc', 'vendor')
                               for scenario in ('task', 'timer', 'fallback', 'send', 'query', 'flush', 'connected'))),
    Requirement('tests/test_usb_reentrancy.py::test_usb_real_header_links', 3,
                boards=('apollo4p_blue_kxr_evb', 'apollo510_evb', 'apollo510b_evb')),
    Requirement('tests/test_system_status.py::test_manual_debug_disable', 3,
                parameters=('system_backend',),
                variants=tuple((family,) for family in ('apollo510', 'apollo330p', 'apollo510l'))),
    Requirement('tests/test_system_status.py::test_debug_failure_smoke_builds', 1,
                boards=('apollo330mP_evb',)),
    Requirement('tests/test_system_status.py::test_apollo510l_debug_backend_compiles'),
    Requirement('tests/test_system_status.py::test_bsp_swo_preparation_errors_return_and_restore', 4,
                parameters=('system_backend', 'masked'),
                variants=tuple((family, masked) for family in ('apollo330p', 'apollo510l') for masked in (0, 1))),
    Requirement('tests/test_debug_state.py::test_apollo2_debug_backend_compiles'),
    Requirement('tests/test_debug_state.py::test_debug_state_real_header_builds', 4,
                boards=('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510b_evb', 'apollo330mP_evb')),
    Requirement('tests/test_system_status.py::test_startup_public_composition', 84,
                parameters=('system_backend', 'scenario'),
                variants=tuple((family, scenario)
                    for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l', 'apollo510', 'apollo330p', 'apollo510l')
                    for scenario in ('full-none', 'full-itm', 'full-uart', 'minimal-none', 'minimal-itm',
                                     'minimal-uart', 'staged-full-itm', 'staged-minimal-itm', 'default',
                                     'none-after-itm', 'fail-core', 'fail-cache'))),
    Requirement('tests/test_system_status.py::test_itm_precedes_high_performance', 6,
                parameters=('system_backend', 'scenario'),
                variants=tuple((family, scenario) for family in ('apollo510', 'apollo330p', 'apollo510l')
                               for scenario in ('full-high-itm', 'minimal-high-itm'))),
    Requirement('tests/test_system_status.py::test_system_performance_mapping', 30,
                parameters=('system_backend', 'mode', 'failure'),
                variants=tuple((family, mode, failure)
                    for family in ('apollo4p', 'apollo4l', 'apollo510', 'apollo330p', 'apollo510l')
                    for mode in (0, 1, 2) for failure in (False, True))),
    Requirement('tests/test_system_status.py::test_additional_system_family_headers_compile', 3,
                boards=('apollo3_evb', 'apollo4l_evb', 'apollo510_evb')),
    Requirement('tests/test_system_status.py::test_system_preflight_and_order', 10,
                parameters=('system_backend', 'scenario'),
                variants=tuple((family, scenario) for family in ('apollo3p', 'apollo4p', 'apollo510', 'apollo330p', 'apollo510l')
                               for scenario in ('invalid', 'success'))),
    Requirement('tests/test_system_status.py::test_minimal_failure_stops_setup', 5,
                parameters=('system_backend',),
                variants=tuple((family,) for family in ('apollo3p', 'apollo4p', 'apollo510', 'apollo330p', 'apollo510l'))),
    Requirement('tests/test_system_status.py::test_dcu_cleanup_and_debug_failures', 6,
                parameters=('system_backend', 'masked'),
                variants=tuple((family, masked) for family in ('apollo510', 'apollo330p', 'apollo510l') for masked in (0, 1))),
    Requirement('tests/test_system_status.py::test_system_status_real_header_builds', 4,
                boards=('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510b_evb', 'apollo330mP_evb')),

    Requirement('tests/test_power_startup.py::test_memory_preflight_smoke_builds'),
    Requirement('tests/test_power_startup.py::test_memory_preflight_without_api_validation', 16,
                parameters=('startup_binary', 'scenario', 'api_validation'),
                variants=tuple((family, f'{small}-{ssram}', False)
                    for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l')
                    for small in (0, 1) for ssram in (0, 1))),
    Requirement('tests/test_power_startup.py::test_memory_preflight', 16,
                parameters=('startup_binary', 'scenario'),
                variants=tuple((family, f'{small}-{ssram}')
                    for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l')
                    for small in (0, 1) for ssram in (0, 1))),
    Requirement('tests/test_power_startup.py::test_usb_policy_smoke_builds', 2,
                parameters=('mode',), variants=(('keep',), ('off',))),
    Requirement('tests/test_power_startup.py::test_peripheral_policy', 84,
                parameters=('startup_binary', 'scenario'),
                variants=tuple((family, scenario) for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l')
                    for scenario in tuple(f'{field}-{value}' for field in
                        ('need_usb', 'need_iom', 'need_uart', 'need_audadc', 'need_crypto',
                         'need_xtal', 'need_itm', 'need_ble', 'spotmgr_collapse') for value in (0, 1)) +
                        ('preset-all_on', 'preset-minimal', 'preset-audio'))),
    Requirement('tests/test_power_startup.py::test_apollo5_power_ownership', 36,
                parameters=('startup_binary', 'scenario', 'api_validation'),
                variants=tuple((family, scenario, validation)
                    for family in ('apollo510', 'apollo510b', 'apollo5a', 'apollo5b', 'apollo330p', 'apollo510l')
                    for scenario in ('power-not-set', 'power-failure', 'power-reentry')
                    for validation in (True, False))),
    Requirement('tests/test_power_startup.py::test_apollo510l_startup_headers'),
    Requirement('tests/test_power_startup.py::test_startup_ownership', 280,
                parameters=('startup_binary', 'scenario', 'api_validation'),
                variants=tuple((family, scenario, validation) for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l', 'apollo510', 'apollo510b', 'apollo5a', 'apollo5b', 'apollo330p', 'apollo510l')
                    for scenario in ('system', 'system-minimal', 'hardware', 'minimal', 'power-first',
                                     'reentry', 'system-failure-1', 'system-failure-2', 'system-failure-3',
                                     'system-failure-4', 'minimal-failure', 'preflight', 'masked', 'masked-failure')
                    for validation in (True, False))),
    Requirement('tests/test_power_startup.py::test_ownership_smoke_builds', 5,
                boards=('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb')),
    Requirement('tests/test_power_startup.py::test_power_startup_contract', 45,
                parameters=('startup_binary', 'scenario'),
                variants=tuple((family, scenario) for family in ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l')
                    for scenario in ('null', 'null-api', 'bad-api', 'bad-version', 'invalid-mode', 'no-core',
                                     'success', 'success-keep', 'fail-minimal', 'fail-keep') +
                                    (('success-tempco', 'fail-tempco') if family == 'apollo4p'
                                     else ('unsupported-tempco',)))),
    Requirement('tests/test_power_startup.py::test_apollo4l_power_sources_compile'),
    Requirement('tests/test_tempco_lifecycle.py::test_tempco_smoke_builds', 2,
                parameters=('capture',), variants=((False,), (True,))),
    Requirement('tests/test_tempco_lifecycle.py::test_tempco_capture', 32,
                parameters=('fault', 'sample', 'sleep_failure'),
                variants=tuple((f, s, e) for f in range(1, 9) for s in (0, 4) for e in (0, 1))),
    Requirement('tests/test_tempco_lifecycle.py::test_tempco_lifecycle', 10,
                parameters=('scenario',), variants=tuple((s,) for s in
                    ('success', 'init-failures', 'cleanup-failures', 'callback-failures', 'callback-status', 'unsupported',
                     'trim-read-failure', 'init-cleanup-failure', 'timer-owner', 'timer-reconfigure'))),
)
PROFILES = {'payload': PAYLOAD, 'gcc': PAYLOAD + GCC + ADAPTER_REGRESSIONS + CORE_BUILDS + POWER_STARTUP}


def belongs(nodeid: str, selector: str) -> bool:
    return nodeid == selector or nodeid.startswith(selector + '::') or nodeid.startswith(selector + '[')


def case_parameters(params: dict) -> dict:
    """Adapt the existing link-smoke fixture's tuple to named matrix dimensions."""
    result = dict(params)
    if 'firmware' in result:
        family, board = result.pop('firmware')
        result.update(family=family, board=board)
    return result


class Gate:
    def __init__(self, requirements: tuple[Requirement, ...]):
        self.requirements = requirements
        self.selected: list[str] = []
        self.excluded: list[str] = []
        self.boards: dict[str, str] = {}
        self.parameters: dict[str, dict] = {}
        self.phases: dict[str, dict[str, str]] = {}

    @pytest.hookimpl(trylast=True)
    def pytest_collection_modifyitems(self, config, items):
        retained, excluded = [], []
        for item in items:
            params = getattr(getattr(item, 'callspec', None), 'params', {})
            # Only compiler-bearing checks in GCC use these parameters. Do not
            # filter unrelated CMake configuration tests (they do not use the target compiler).
            required_compilers = {r.compiler for r in self.requirements
                                  if belongs(item.nodeid, r.selector) and r.compiler}
            params = case_parameters(params)
            if required_compilers and params.get('family') not in required_compilers:
                excluded.append(item)
            else:
                retained.append(item)
                self.parameters[item.nodeid] = params
                board = params.get("board")
                if board:
                    self.boards[item.nodeid] = board
        items[:] = retained
        config.hook.pytest_deselected(items=excluded)
        self.selected = [item.nodeid for item in retained]
        self.excluded = [item.nodeid for item in excluded]

    def pytest_runtest_logreport(self, report):
        self.phases.setdefault(report.nodeid, {})[report.when] = report.outcome

    def report(self, exit_code: int) -> dict:
        missing = [r.selector for r in self.requirements
                   if sum(belongs(n, r.selector) for n in self.selected) < r.minimum]
        missing_boards = {r.selector: sorted(set(r.boards) - {
            self.boards.get(n) for n in self.selected if belongs(n, r.selector)})
            for r in self.requirements if r.boards}
        missing_boards = {selector: boards for selector, boards in missing_boards.items() if boards}
        missing_variants = {}
        for requirement in self.requirements:
            observed = {tuple(self.parameters.get(n, {}).get(p) for p in requirement.parameters)
                        for n in self.selected if belongs(n, requirement.selector)}
            absent = [v for v in requirement.variants if v not in observed]
            if absent:
                missing_variants[requirement.selector] = absent
        statuses = {n: phase_status(self.phases.get(n, {})) for n in self.selected}
        incomplete = [n for n, status in statuses.items() if status != 'passed']
        status = completion(statuses.values(), session_ok=exit_code == 0,
                            requirements_met=not (missing or missing_boards or missing_variants))
        return {'status': status, 'pytest_exit_code': int(exit_code),
                'missing_requirements': missing, 'missing_boards': missing_boards, 'missing_variants': missing_variants, 'incomplete_cases': incomplete,
                'selected_count': len(self.selected), 'cases': self.phases,
                'excluded_other_compilers': self.excluded,
                'requirements': [{'selector': r.selector, 'minimum': r.minimum, 'boards': r.boards,
                                  'parameters': r.parameters, 'variants': r.variants, 'compiler': r.compiler}
                                 for r in self.requirements]}


def identity(repo: Path) -> dict:
    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()
    paths = ('modules/nsx-ambiqsuite/nsx-module.yaml',
             'modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml',
             'modules/nsx-ambiqsuite/sdk/mcu/am_sdk_version.h')
    return {'revision': git('rev-parse', 'HEAD'), 'dirty': bool(git('status', '--porcelain')),
            'sdk_identity_sha256': {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in paths}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory (must not exist)')
    args = parser.parse_args()
    if os.environ.get('PYTEST_ADDOPTS', '').strip():
        parser.error('Unset PYTEST_ADDOPTS so it cannot alter required coverage')
    repo = Path(__file__).resolve().parents[1]
    if Path.cwd().resolve() != repo:
        parser.error('Run from the candidate SDK repository root')
    output = args.output.resolve()
    if output.exists():
        parser.error('--output must be a new directory; preserve prior run evidence')
    output.mkdir(parents=True)
    report_path = output / 'compatibility.json'
    report = {'schema_version': 1, 'profile': args.profile, 'status': 'incomplete'}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    gate = Gate(PROFILES[args.profile])
    code = 2
    try:
        report['candidate'] = identity(repo)
        report['python'] = sys.version
        if args.profile == 'gcc':
            compiler = find_gcc(None)
            report['gcc'] = None if compiler is None else {
                'path': str(compiler),
                'version': subprocess.check_output([str(compiler), '--version'],
                                                 text=True, timeout=15).splitlines()[0]}

        code = pytest.main(['-q', '-o', 'addopts=', *[r.selector for r in gate.requirements],
                            f'--junitxml={output / "junit.xml"}',
                            f'--basetemp={output / "pytest-work"}'], plugins=[gate])
        report.update(gate.report(code))
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(f'Compatibility report: {report_path} ({report["status"]})')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
