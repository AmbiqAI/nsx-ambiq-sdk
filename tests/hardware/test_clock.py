"""Experimental board checks; not an approved frequency qualification profile."""
import pytest
from tools.hardware.suites import CLOCK


@pytest.fixture
def suite_spec(request):
    if not request.config.getoption('--clock-experimental'):
        pytest.skip('Clock diagnostic is experimental: requires --clock-experimental')
    if request.config.getoption('--qualification-profile'):
        raise pytest.UsageError('Experimental clock tests cannot be used in qualification profiles')
    return CLOCK


@pytest.mark.hardware
def test_clock(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
