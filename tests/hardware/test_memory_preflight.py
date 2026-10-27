"""Reject unproven memory reductions before touching live application memory."""
import pytest
from tools.hardware.suites import MEMORY_PREFLIGHT


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') != 'apollo4p_blue_kxr_evb':
        pytest.skip('Memory preflight smoke supports AP4 Blue KXR')
    return MEMORY_PREFLIGHT


@pytest.mark.hardware
@pytest.mark.smoke
def test_memory_preflight(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
