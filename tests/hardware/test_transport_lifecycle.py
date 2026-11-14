"""Explicit physical cleanup checks; not positive qualification suite rows."""
import pytest

from tools.hardware.lifecycle import run_control


@pytest.mark.hardware
@pytest.mark.parametrize('action', ('term', 'interrupt', 'timeout'))
def test_transport_lifecycle(repo_root, run_config, action, request):
    if not request.config.getoption('--transport-controls'):
        pytest.skip('Physical failure/recovery controls require --transport-controls')
    if request.config.getoption('--qualification-profile'):
        raise pytest.UsageError('Transport controls are separate from positive qualification profiles')
    if not run_config.hardware:
        pytest.skip('Physical transport controls require --hardware')
    if run_config.fault != 'none':
        raise pytest.UsageError('Lifecycle controls select their own withheld-completion fault')
    output = run_config.artifacts / run_config.board / f'lifecycle-{action}'
    receipt = run_control(repo_root, run_config.board, run_config.gcc_root,
                          run_config.board_request, output, action, run_config.sdk_root)
    assert receipt['status'] == 'passed'
