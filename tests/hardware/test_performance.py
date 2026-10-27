"""Board smoke for the two public performance selectors and their timebase seam."""
import pytest
from tools.hardware.suites import PERFORMANCE


@pytest.fixture
def suite_spec():
    return PERFORMANCE


@pytest.mark.hardware
def test_performance(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
