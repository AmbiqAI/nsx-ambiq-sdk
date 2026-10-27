"""One real system startup followed by rejection of repeated startup entry points."""
import pytest
from tools.hardware.suites import OWNERSHIP


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in ('apollo3p_evb', 'apollo4p_blue_kxr_evb'):
        pytest.skip('Startup ownership smoke supports AP3 Blue Plus and AP4 Blue KXR')
    return OWNERSHIP


@pytest.mark.hardware
@pytest.mark.smoke
def test_ownership(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
