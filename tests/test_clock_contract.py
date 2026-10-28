"""Independent worst-case calculation for the documented conditional screen.

This validates the budget arithmetic, not the assembled EVB reference accuracy.
"""
from fractions import Fraction
from itertools import product

import pytest

NOMINAL_HZ = (96_000_000, 192_000_000, 250_000_000)
SOURCE_ERROR = Fraction(5, 100)
REFERENCE_ERROR = Fraction(50, 1_000_000)
SCREEN_ERROR = Fraction(52, 1000)
REFERENCE_HZ = 32768
WINDOW = 4096
REFERENCE_ENDPOINT_ERROR = 2
CYCLE_ENDPOINT_ERROR = 2 * 2048


@pytest.mark.parametrize('nominal', NOMINAL_HZ)
def test_conditional_frequency_screen_covers_combined_error(nominal):
    # C = physical CPU Hz * elapsed reference interval / physical reference Hz
    #     + cycle capture error. Test all independent endpoint signs, including
    # opposing CPU/reference errors, rather than just summing percentages.
    estimates = []
    for cpu, reference, endpoint, aperture in product((-1, 1), repeat=4):
        physical_hz = nominal * (1 + cpu * SOURCE_ERROR)
        physical_reference = REFERENCE_HZ * (1 + reference * REFERENCE_ERROR)
        cycles = (physical_hz * (WINDOW + endpoint * REFERENCE_ENDPOINT_ERROR)
                  / physical_reference + aperture * CYCLE_ENDPOINT_ERROR)
        estimates.append(cycles * REFERENCE_HZ / WINDOW)
    assert min(estimates) >= nominal * (1 - SCREEN_ERROR)
    assert max(estimates) <= nominal * (1 + SCREEN_ERROR)


@pytest.mark.parametrize('low,high', [(96_000_000, 192_000_000), (192_000_000, 250_000_000)])
def test_screen_does_not_hide_an_adjacent_nominal_mode(low, high):
    assert low * (1 + SCREEN_ERROR) < high * (1 - SCREEN_ERROR)


def test_capture_window_cannot_contain_a_cycle_counter_revolution():
    minimum_reference_hz = 32766  # Conservative floor for the stated ±50 ppm.
    assert minimum_reference_hz <= REFERENCE_HZ * (1 - REFERENCE_ERROR)
    maximum_window = WINDOW + 8
    assert (Fraction(300_000_000 * (maximum_window + REFERENCE_ENDPOINT_ERROR),
                     minimum_reference_hz) + CYCLE_ENDPOINT_ERROR) < 2**32
    # Separate in-loop cycle deadline; this is not a host timeout in disguise.
    assert 300_000_000 < 2**32
