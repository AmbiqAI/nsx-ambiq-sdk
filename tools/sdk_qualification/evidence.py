"""Revalidate concrete inputs and raw results; never infer qualification from PASS text."""
from dataclasses import asdict
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from tools.hardware.protocol import RESULT_SIZE, decode
from tools.hardware.suites import SUITES
from . import abi
from .io import contained, read_json, sha256
from .models import Row
from .runtime import Tools

SDK_IDENTITIES = ('modules/nsx-ambiqsuite/nsx-module.yaml',
                  'modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml',
                  'modules/nsx-ambiqsuite/sdk/mcu/am_sdk_version.h')


def verify_files(directory: Path, files: dict) -> None:
    if not isinstance(files, dict):
        raise ValueError('Expected evidence file inventory')
    for name, expected in files.items():
        if sha256(contained(directory, name)) != expected:
            raise ValueError(f'Evidence file changed: {name}')


def tool_fingerprint(value: dict) -> dict:
    items = value['binaries']
    result = {item['name']: (item['sha256'], item['version']) for item in items}
    if len(items) != len(result):
        raise ValueError('Duplicate tool identities')
    return result


def _relative(path: str, base: str) -> str:
    value = Path(path)
    if not value.is_absolute() or '..' in value.parts:
        raise ValueError('Expected canonical absolute provenance path')
    return str(value.relative_to(base))


def input_path(path: str, role: str, origin: dict, current: dict, original_output: str,
               output: Path, side: str, old_tools: dict, tools: Tools) -> Path:
    if role in ('harness', 'sdk'):
        name = 'harness' if role == 'harness' else side
        return contained(Path(current[name]['root']), _relative(path, origin[name]['root']))
    if role == 'generated':
        return contained(output, _relative(path, original_output))
    if role != 'environment':
        raise ValueError('Unknown input role')
    for old, new in ((old_tools['gcc_root'], tools.gcc_root), (old_tools['cmake_root'], tools.cmake_root)):
        if Path(path).is_relative_to(old):
            return contained(Path(new), _relative(path, old))
    # Nonrelocated system inputs retain their recorded path and exact content.
    if not Path(path).is_absolute() or '..' in Path(path).parts:
        raise ValueError('Unknown environment input')
    return Path(path)


def verify_firmware(directory: Path, entry: dict, row: Row, side: str,
                    origin: dict, current: dict, old_tools: dict, tools: Tools, command) -> dict:
    suite = next(s for s in SUITES if s.name == row.suite)
    required = {'firmware.json', 'manifest.json', 'abi.json', 'compile_commands.json', 'inputs.txt', 'probe.txt', suite.elf_name}
    if type(entry['hardware']) is not bool:
        raise ValueError('Hardware execution must be explicit')
    if entry['hardware']:
        required.update(('execution.json', 'seeded.json', 'jlink.json', 'last-result.bin', 'result.json'))
    if not required.issubset(entry['files']):
        raise ValueError('Missing required firmware evidence files')
    verify_files(directory, entry['files'])
    manifest = read_json(directory / 'manifest.json')
    if type(manifest['schema_version']) is not int or manifest['schema_version'] != 3:
        raise ValueError('Unsupported build manifest')
    for role, key in (('harness', 'harness'), (side, 'sdk')):
        data = manifest[key]
        if data['dirty'] is not False or data['revision'] != current[role]['revision']:
            raise ValueError(f'Firmware {key} identity mismatch')
    if (manifest['board'] != row.board or manifest['suite'] != row.suite
            or manifest['toolchain'] != row.toolchain or manifest['fault_injection'] != 'none'
            or manifest['expected_cases'] != list(row.cases)):
        raise ValueError('Firmware does not match the planned board/suite/cases')
    if (manifest['build_id'] != entry['build_id']
            or not re.fullmatch('[0-9a-f]{32}', manifest['build_id'])):
        raise ValueError('Invalid firmware build identity')
    gcc = next(item for item in old_tools['binaries'] if item['name'] == 'gcc')
    if manifest['compiler'] != gcc['version']:
        raise ValueError('Firmware compiler identity mismatch')
    sdk = Path(current[side]['root'])
    expected_sdk = {name: sha256(sdk / name) for name in SDK_IDENTITIES}
    if manifest['sdk_identity_sha256'] != expected_sdk:
        raise ValueError('Firmware SDK provider identity mismatch')
    roles = manifest['input_roles']
    hashes = manifest['inputs_sha256']
    if set(roles) != {'harness', 'sdk', 'generated', 'environment'} or not hashes:
        raise ValueError('Missing input provenance')
    indexed = [(path, role) for role, paths in roles.items() for path in paths]
    if len(indexed) != len(hashes) or {p for p, _ in indexed} != set(hashes):
        raise ValueError('Incomplete or conflicting input roles')
    for path, role in indexed:
        actual = input_path(path, role, origin, current, entry['origin'], directory, side, old_tools, tools)
        if sha256(actual) != hashes[path]:
            raise ValueError(f'Build input changed: {path}')
    elf = contained(directory, entry['elf'])
    if entry['elf'] != suite.elf_name or sha256(elf) != manifest['elf_sha256']:
        raise ValueError('Linked firmware changed')
    if (directory / 'probe.txt').read_text().splitlines() != [manifest['device'], str(manifest['swd_speed_khz'])]:
        raise ValueError('Probe descriptor differs from its build manifest')
    descriptor = read_json(directory / 'firmware.json')
    expected = {'schema_version': 1, 'elf': suite.elf_name, 'address': descriptor['address'],
                'build_id': manifest['build_id'], 'board': row.board, 'device': manifest['device'],
                'speed': manifest['swd_speed_khz'], 'cases': list(row.cases), 'seeds': manifest['seeds']}
    if descriptor != expected or type(descriptor['address']) is not int or descriptor['address'] < 0:
        raise ValueError('Firmware descriptor differs from its build')
    symbols = command([str(tools.path('nm')), '-S', str(elf)], 'verify-firmware-symbols')
    def symbol(name, size):
        selected = [line.split() for line in symbols.splitlines() if line.endswith(' ' + name)]
        if len(selected) != 1 or len(selected[0]) != 4 or int(selected[0][1], 16) != size:
            raise ValueError(f'Missing or wrong-sized firmware symbol: {name}')
        return int(selected[0][0], 16)
    if symbol('nsx_test_result', RESULT_SIZE) != descriptor['address']:
        raise ValueError('Result address differs from the linked firmware')
    expected_seeds = {seed.symbol: seed.value for seed in suite.seeds}
    if {seed['symbol']: seed['value'] for seed in descriptor['seeds']} != expected_seeds:
        raise ValueError('Firmware seed contract differs from the fixed harness')
    if len(descriptor['seeds']) != len(expected_seeds):
        raise ValueError('Duplicate firmware seeds')
    for seed in descriptor['seeds']:
        if symbol(seed['symbol'], 4) != seed['address']:
            raise ValueError('Seed address differs from the linked firmware')
    inputs = (directory / 'inputs.txt').read_text().splitlines()
    if len(inputs) != 5:
        raise ValueError('Missing selected HAL/BSP inputs')
    archives = []
    for path in inputs[-2:]:
        if path not in hashes:
            raise ValueError('Selected archive absent from input inventory')
        role = next(role for name, role in indexed if name == path)
        if role != 'sdk':
            raise ValueError('Selected HAL/BSP archive is outside the SDK')
        archives.append(input_path(path, role, origin, current, entry['origin'], directory, side, old_tools, tools))
    abi.verify(directory, tools, archives, command)
    if entry['hardware']:
        execution = read_json(directory / 'execution.json')
        binding = entry['binding']
        if (execution['status'] != 'passed' or execution['board'] != row.board
                or execution['device'] != manifest['device'] or execution['device'] != binding['device']
                or execution['probe'] != binding['probe'] or binding['board'] != row.board
                or execution['swd_speed_khz'] != manifest['swd_speed_khz']
                or execution['debugger_connected'] is not True or execution['cold_boot'] is not False
                or execution['reset'] != 'jlink-reset-before-flash-reset-halt-then-run'):
            raise ValueError('Hardware execution identity/policy mismatch')
        if read_json(directory / 'seeded.json') != {seed.symbol: seed.value for seed in suite.seeds}:
            raise ValueError('Missing verified pre-execution seeds')
        jlink = read_json(directory / 'jlink.json')
        if jlink['device'] != manifest['device'] or not jlink['version']:
            raise ValueError('Missing J-Link metadata')
        result = decode((directory / 'last-result.bin').read_bytes(), manifest['build_id'], row.cases)
        if result is None:
            raise ValueError('Incomplete raw target result')
        if read_json(directory / 'result.json') != asdict(result):
            raise ValueError('Decoded target report differs from raw samples')
    return manifest


def verify_host(directory: Path, host: dict, roots: dict, old_tools: dict, tools: Tools) -> None:
    from tools.sdk_compatibility import Gate, PROFILES, belongs
    from tools.sdk_intake_review import review as review_snapshots
    review = read_json(contained(directory, host['review']))
    if review['schema_version'] != 1 or review['status'] != 'review-recorded':
        raise ValueError('Missing recorded intake review')
    for side in ('baseline', 'candidate'):
        if review[side]['revision'] != roots[side]['revision']:
            raise ValueError('Intake review identity mismatch')
    checked = review_snapshots(Path(roots['baseline']['root']), Path(roots['candidate']['root']), review['decisions'])
    for side in ('baseline', 'candidate'):
        keys = ('revision', 'upstream_commit', 'source_commit', 'inventory_sha256')
        if any(checked[side][key] != review[side][key] for key in keys):
            raise ValueError('Intake source/patch/archive inventory changed')
    result = read_json(contained(directory, host['compatibility']))
    if type(result['schema_version']) is not int or result['schema_version'] != 2 or result['profile'] != 'gcc':
        raise ValueError('Candidate requires compatibility producer schema 2 with observations')
    candidate = result['candidate']
    if candidate['revision'] != roots['candidate']['revision'] or candidate['dirty'] is not False:
        raise ValueError('Host gate candidate identity mismatch')
    if candidate['sdk_identity_sha256'] != {name: sha256(Path(roots['candidate']['root']) / name) for name in SDK_IDENTITIES}:
        raise ValueError('Host SDK identity changed')
    expected_gcc = next(item for item in tools.binaries if item.name == 'gcc')
    original_gcc = next(item for item in old_tools['binaries'] if item['name'] == 'gcc')
    if result['gcc']['version'] != expected_gcc.version or result['gcc']['path'] != original_gcc['path']:
        raise ValueError('Host gate used a different compiler')
    expected_requirements = json.loads(json.dumps([asdict(req) for req in PROFILES['gcc']]))
    if result['requirements'] != expected_requirements:
        raise ValueError('Candidate host requirements differ from the fixed harness')
    gate = Gate(PROFILES['gcc'])
    gate.selected = list(result['cases'])
    gate.phases = result['cases']
    gate.parameters = result['observations']
    if set(gate.selected) != set(gate.parameters) or result['selected_count'] != len(gate.selected):
        raise ValueError('Missing host case observations')
    for node, params in gate.parameters.items():
        if 'board' in params:
            gate.boards[node] = params['board']
        for requirement in gate.requirements:
            if belongs(node, requirement.selector) and requirement.compiler:
                if params.get('family') != requirement.compiler:
                    raise ValueError('Wrong compiler case in host gate')
    if result['status'] != 'passed' or gate.report(result['pytest_exit_code'])['status'] != 'passed':
        raise ValueError('Host compatibility coverage did not pass')
    tests = list(ET.parse(contained(directory, host['junit'])).getroot().iter('testcase'))
    if (not tests or all(case.find('skipped') is not None for case in tests)
            or any(case.find('failure') is not None or case.find('error') is not None for case in tests)):
        raise ValueError('Full host tests did not pass')


def verify_header(plan, body: dict, roots: dict, tools: Tools) -> None:
    keys = {'execution_id', 'plan_sha256', 'origin_output', 'revisions', 'roots', 'tools', 'scope',
            'scope_status', 'qualification', 'global_status', 'inputs_rechecked', 'firmware', 'host',
            'host_reference', 'calls', 'started_at', 'completed_at', 'elapsed_seconds', 'cleanup', 'error', 'files'}
    if set(body) != keys or body['plan_sha256'] != plan.digest or body['revisions'] != asdict(plan.revisions):
        raise ValueError('Execution record does not match its plan/schema')
    if not re.fullmatch('[0-9a-f]{32}', body['execution_id']) or not Path(body['origin_output']).is_absolute():
        raise ValueError('Invalid execution identity/original output')
    if body['qualification'] is not False or body['global_status'] not in ('incomplete', 'failed'):
        raise ValueError('An execution record cannot assert full qualification')
    if body['scope_status'] != 'passed' or body['inputs_rechecked'] is not True or body['error'] is not None:
        raise ValueError('Execution did not complete successfully with stable inputs')
    if set(body['roots']) != set(roots):
        raise ValueError('Missing source roots')
    for role, current in roots.items():
        value = body['roots'][role]
        if value['revision'] != current['revision'] or value['dirty'] is not False or not Path(value['root']).is_absolute():
            raise ValueError('Execution source revision/dirty state mismatch')
    if tool_fingerprint(body['tools']) != tools.fingerprint():
        raise ValueError('Execution and assessment tool identities differ')
    scope = body['scope']
    if (set(scope) != {'mode', 'boards'} or scope['mode'] not in ('host', 'build', 'hardware')
            or not isinstance(scope['boards'], list) or len(set(scope['boards'])) != len(scope['boards'])):
        raise ValueError('Invalid execution scope')
    from .policy import BOARDS
    if any(board not in BOARDS for board in scope['boards']):
        raise ValueError('Unknown scope board')
    if (scope['mode'] == 'host') != (not scope['boards']):
        raise ValueError('Host scopes have no boards; build/hardware scopes require boards')
    if scope['mode'] == 'hardware' and len(scope['boards']) != 1:
        raise ValueError('Hardware records describe exactly one board')
    expected_state = 'tests-completed' if scope['mode'] == 'hardware' else 'not-accessed'
    if body['cleanup'] != {'process_groups': 'completed', 'board_state': expected_state}:
        raise ValueError('Missing completed process/board lifecycle')
    if body['global_status'] != 'incomplete':
        raise ValueError('Successful scoped execution must remain globally incomplete')
    if not isinstance(body['calls'], list) or not body['calls']:
        raise ValueError('Missing executed commands')
    for call in body['calls']:
        if (call['outcome'] != 'passed' or type(call['returncode']) is not int
                or call['returncode'] != 0 or call['log'] not in body['files']):
            raise ValueError('Unfinished command or missing command log')
    if scope['mode'] == 'host':
        labels = [call['label'] for call in body['calls']]
        if labels != ['intake-review', 'candidate-host-gcc', 'candidate-hygiene', 'candidate-host-tests']:
            raise ValueError('Host record lacks required command completion')
        if body['firmware'] or body['host_reference'] is not None or not isinstance(body['host'], dict):
            raise ValueError('Invalid host-only record')
    elif body['host'] is not None or (scope['mode'] == 'hardware') != (body['host_reference'] is not None):
        raise ValueError('Hardware requires prior host acceptance; build has no host gate')
