"""Compile the runtime suite against real family targets; no probe access."""
import json
import subprocess

import pytest

from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.inventory import BOARDS
from tools.hardware.qualification import PROFILES
from tools.hardware.suites import RUNTIME, STARTUP
from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize('board', BOARDS)
def test_board_profile_has_shipped_descriptor(repo_root, board):
    assert (repo_root / 'boards' / board / 'board.cmake').is_file()
    assert PROFILES[f'{board}-gcc-core'].suites == (STARTUP, RUNTIME)
    assert PROFILES[f'{board}-gcc-startup'].suites == (STARTUP,)


@pytest.mark.parametrize('board', ['apollo3p_evb', 'apollo4p_blue_kbr_evb', 'apollo4p_blue_kxr_evb',
                                   'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb'])
def test_runtime_links_real_handlers_and_heap(repo_root, tmp_path, board):
    compiler = compiler_for_test('gcc')
    image = build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, RUNTIME))
    listing = subprocess.check_output([str(compiler.parent / 'arm-none-eabi-nm'), str(image.elf)], text=True)
    symbols = {parts[2]: (int(parts[0], 16), parts[1]) for line in listing.splitlines()
               if len(parts := line.split()) == 3}
    assert symbols['SVC_Handler'][1] == 'T'
    assert symbols['_sbrk'][1] == 'T'  # Strong NSX bounded implementation, not the weak vendor stub.
    assert symbols['__nsx_test_stack_start'][0] < symbols['__nsx_test_stack_end'][0]
    assert symbols['__HeapBase'][0] < symbols['__HeapLimit'][0]
    manifest = json.loads((tmp_path / 'manifest.json').read_text())
    assert any(path.endswith('/nsx-core/src/sbrk.c') for path in manifest['inputs_sha256'])
    assert manifest['expected_cases'] == list(RUNTIME.cases)


def test_runtime_rejects_unknown_negative_control(repo_root, tmp_path):
    with pytest.raises(ValueError, match='Unknown runtime'):
        RUNTIME.configure(repo_root, tmp_path, 'apollo3p_evb', 'omit-data')
