import pytest

from tools.hardware.suites import RUNTIME


@pytest.fixture
def suite_spec():
    return RUNTIME


@pytest.mark.hardware
@pytest.mark.smoke
def test_runtime(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
