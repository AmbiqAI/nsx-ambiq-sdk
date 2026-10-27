"""Register-level debug lifecycle smoke; external SWO capture is separate."""
import pytest
from tools.hardware.suites import DEBUG_STATE


@pytest.fixture
def suite_spec():
    return DEBUG_STATE


@pytest.mark.hardware
@pytest.mark.smoke
def test_debug_output(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
