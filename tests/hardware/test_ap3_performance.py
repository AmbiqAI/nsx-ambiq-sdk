"""AP3 mixed public selectors and normal-divider hardware smoke."""
import pytest
from tools.hardware.suites import AP3_PERFORMANCE


@pytest.fixture
def suite_spec():
    return AP3_PERFORMANCE


@pytest.mark.hardware
def test_ap3_performance(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
