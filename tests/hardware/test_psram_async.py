"""Opt-in direct DTCM DMA, per-client completion and XIP coexistence checks."""
import pytest
from tools.hardware.suites import PSRAM_ASYNC_MATRIX, PSRAM_BOARDS


@pytest.fixture(params=PSRAM_ASYNC_MATRIX, ids=lambda suite: suite.name)
def suite_spec(request):
    if request.config.getoption('--board') not in PSRAM_BOARDS:
        pytest.skip('Async PSRAM requires AP510 or AP510B')
    return request.param


@pytest.mark.hardware
@pytest.mark.smoke
def test_psram_async(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
