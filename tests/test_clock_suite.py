"""Build real clock firmware and keep provisional work out of qualification."""
import json

import pytest

from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.suites import CLOCK, CLOCK_BOARDS, CLOCK_FAULTS, clock_options
from tools.hardware.qualification import PROFILES
from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize('board', CLOCK_BOARDS)
@pytest.mark.parametrize('fault', CLOCK_FAULTS)
def test_clock_firmware_builds(repo_root, tmp_path, board, fault):
    compiler = compiler_for_test('gcc')
    fw = build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, CLOCK, fault))
    manifest = json.loads((tmp_path / 'manifest.json').read_text())
    assert fw.cases == ('reference_ready', 'clock_transitions', 'restore_low')
    assert manifest['fault_injection'] == fault
    assert any(p.endswith('nsx_power_performance.c') for p in manifest['inputs_sha256'])
    assert any(p.endswith('/libam_hal.a') for p in manifest['inputs_sha256'])


def test_experimental_clock_not_a_qualification_profile():
    assert all(CLOCK not in p.suites for p in PROFILES.values())


def test_clock_configuration_rejects_unsupported_inputs(tmp_path):
    with pytest.raises(ValueError, match='AP510B and AP330'):
        clock_options(tmp_path, tmp_path, 'apollo3p_evb', 'none')
    with pytest.raises(ValueError, match='Unknown clock fault'):
        clock_options(tmp_path, tmp_path, 'apollo510b_evb', 'typo')
