"""Exercise public power startup on real Apollo5-class hardware."""
import pytest
from tools.hardware.suites import POWER_STARTUP


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in ('apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb'):
        pytest.skip('Power startup smoke requires AP510/B or AP330 Plus')
    return POWER_STARTUP


@pytest.mark.hardware
@pytest.mark.smoke
def test_power_startup(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
