"""Opt-in comparative timing; correctness is mandatory, rates are observations."""
import pytest
from tools.hardware.suites import PSRAM_BOARDS, PSRAM_THROUGHPUT


@pytest.fixture(params=PSRAM_THROUGHPUT, ids=lambda suite: suite.name)
def suite_spec(request):
    if request.config.getoption('--board') not in PSRAM_BOARDS:
        pytest.skip('PSRAM throughput diagnostic requires AP510 or AP510B')
    return request.param


@pytest.mark.hardware
def test_psram_throughput(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
