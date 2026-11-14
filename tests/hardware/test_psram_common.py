"""Shared v1 positive path; candidate cache/alignment policy stays in its own suite."""
import pytest
from tools.hardware.suites import PSRAM_COMMON, PSRAM_BOARDS


@pytest.fixture
def suite_spec(request):
    if request.config.getoption('--board') not in PSRAM_BOARDS:
        pytest.skip('Common PSRAM contract requires AP510 or AP510B')
    return PSRAM_COMMON


@pytest.mark.hardware
@pytest.mark.smoke
def test_psram_common(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
