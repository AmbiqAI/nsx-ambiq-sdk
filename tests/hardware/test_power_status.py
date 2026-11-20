"""Read live status before, during and after an independent IOM0 power request."""
import pytest

from tools.hardware.suites import POWER_STATUS


@pytest.fixture
def suite_spec():
    return POWER_STATUS


@pytest.mark.hardware
@pytest.mark.smoke
def test_power_status(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
