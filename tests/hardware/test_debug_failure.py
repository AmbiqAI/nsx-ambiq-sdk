"""Safe returning-error injection at the AP330/510L SWO preparation boundary."""
import pytest
from tools.hardware.suites import DEBUG_FAILURE


@pytest.fixture
def suite_spec():
    return DEBUG_FAILURE


@pytest.mark.hardware
@pytest.mark.smoke
def test_debug_failure(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
