"""Bounded TIMER callback-period smoke; not calibrated oscillator qualification."""
import pytest
from tools.hardware.suites import TIMER_PERIOD, TIMER_BOARDS


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in TIMER_BOARDS:
        pytest.skip('Timer-period smoke requires an Apollo4/510/330 TIMER backend')
    return TIMER_PERIOD


@pytest.mark.hardware
@pytest.mark.smoke
def test_timer_period(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
