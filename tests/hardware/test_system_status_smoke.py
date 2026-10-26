"""Invalid requests followed by real full/minimal startup and ITM configuration."""
import pytest

from tools.hardware.suites import SYSTEM_FULL, SYSTEM_MINIMAL


@pytest.fixture(params=(SYSTEM_FULL, SYSTEM_MINIMAL), ids=lambda suite: suite.name)
def suite_spec(request):
    return request.param


@pytest.mark.hardware
@pytest.mark.smoke
def test_system_status(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
