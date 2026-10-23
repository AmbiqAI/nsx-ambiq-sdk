"""Board selection must preserve the distinction between Apollo510 and 510B."""
import pytest

from test_cmake_contract import configure_contract_project


@pytest.mark.parametrize('board,expected', [('apollo510_evb', 'AP510NFA-CBR'),
                                            ('apollo510b_evb', 'AP510BFA-CBR')])
def test_apollo510_probe_profiles(repo_root, tmp_path, board, expected):
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc',
        post_board_include=('file(WRITE "${CMAKE_BINARY_DIR}/selected-device.txt" "${NSX_SEGGER_DEVICE}")',))
    assert result.returncode == 0, result.stdout
    assert (tmp_path / 'gcc' / board / 'build/selected-device.txt').read_text() == expected
