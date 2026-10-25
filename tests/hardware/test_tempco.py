"""Bounded resource lifecycle smoke, not temperature/trim qualification."""
import pytest
from tools.hardware.suites import TEMPCO


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') != 'apollo4p_blue_kxr_evb':
        pytest.skip('TempCo lifecycle smoke currently supports AP4 Blue Plus KXR only')
    return TEMPCO


@pytest.mark.hardware
@pytest.mark.smoke
def test_tempco(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
