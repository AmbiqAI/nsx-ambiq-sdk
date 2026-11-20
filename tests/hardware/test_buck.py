"""Raw buck counter measurements with the fixture controlling regulator policy."""
import pytest

from tools.hardware.suites import BUCK, BUCK_BOARDS, BUCK_UNSUPPORTED, BUCK_UNSUPPORTED_BOARDS


@pytest.fixture
def suite_spec(request):
    board = request.config.getoption('--board')
    if request.node.originalname == 'test_unsupported':
        if board not in BUCK_UNSUPPORTED_BOARDS:
            pytest.skip('Buck pulse unsupported-target fixture requires a gated target')
        return BUCK_UNSUPPORTED
    if board not in BUCK_BOARDS:
        pytest.skip('Buck pulse fixture requires a supported target')
    return BUCK


@pytest.mark.hardware
@pytest.mark.smoke
def test_buck(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)


@pytest.mark.hardware
@pytest.mark.smoke
def test_unsupported(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
