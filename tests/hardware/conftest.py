import pytest

from tools.hardware.suites import STARTUP


@pytest.fixture
def suite_spec():
    return STARTUP
