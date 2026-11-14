"""Run the existing full-system assertions through the built-in BSP v2 adapter."""
import pytest
from tools.hardware.suites import BSP_V2_SYSTEM


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in ('apollo330mP_evb', 'apollo510_evb'):
        pytest.skip('Built-in BSP v2 is qualified on AP330P/AP510')
    return BSP_V2_SYSTEM


@pytest.mark.hardware
@pytest.mark.smoke
def test_bsp_v2(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
