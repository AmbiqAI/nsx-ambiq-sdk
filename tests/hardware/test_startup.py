import pytest


@pytest.mark.hardware
@pytest.mark.smoke
def test_startup(suite_result, suite_spec):
    assert suite_result.total == len(suite_spec.cases)
