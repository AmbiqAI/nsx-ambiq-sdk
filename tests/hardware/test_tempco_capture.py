"""Actual AP4 sampling callbacks, not temperature/analog qualification."""
import pytest
from tools.hardware.suites import TEMPCO_CAPTURE


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') != 'apollo4p_blue_kxr_evb':
        pytest.skip('TempCo capture smoke supports AP4 Blue Plus KXR only')
    return TEMPCO_CAPTURE


@pytest.mark.hardware
@pytest.mark.smoke
def test_tempco_capture(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
