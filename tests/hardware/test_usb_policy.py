"""Check USB domain policy using real HAL power state, without USB traffic."""
import pytest
from tools.hardware.suites import USB_KEEP, USB_OFF


@pytest.fixture(params=(USB_KEEP, USB_OFF), ids=lambda suite: suite.name)
def suite_spec(request):
    if request.config.getoption('--board') != 'apollo4p_blue_kxr_evb':
        pytest.skip('USB policy smoke supports AP4 Blue KXR')
    return request.param


@pytest.mark.hardware
@pytest.mark.smoke
def test_usb_policy(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
