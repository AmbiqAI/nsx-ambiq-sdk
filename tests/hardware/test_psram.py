"""Opt-in Apollo5 blocking PSRAM ownership and chunk-boundary smoke."""
import pytest
from tools.hardware.suites import PSRAM_BLOCKING, PSRAM_BOARDS


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in PSRAM_BOARDS:
        pytest.skip('PSRAM round trips require AP510 or AP510B')
    return PSRAM_BLOCKING


@pytest.mark.hardware
@pytest.mark.smoke
def test_psram_blocking(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
