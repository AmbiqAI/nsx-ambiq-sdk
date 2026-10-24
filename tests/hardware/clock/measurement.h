#ifndef NSX_TEST_CLOCK_MEASUREMENT_H
#define NSX_TEST_CLOCK_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t cpu_cycles;
    uint32_t reference_ticks;
} nsx_clock_sample_t;

/* Caller must bound the capture duration independently (including a stalled
 * reference), verify the reference source, and avoid debugger halts. Two
 * snapshots alone cannot detect multiple wraps or a stopped clock. */
static inline bool nsx_clock_sample(
    uint32_t cycle_start, uint32_t cycle_end,
    uint32_t reference_start, uint32_t reference_end,
    uint32_t maximum_reference_ticks, uint32_t maximum_cpu_hz,
    uint32_t minimum_reference_hz, nsx_clock_sample_t *sample)
{
    if (!sample || !maximum_reference_ticks || !maximum_cpu_hz || !minimum_reference_hz) {
        return false;
    }
    /* Prove that the permitted window cannot contain a full CPU-counter turn.
     * Use the reference's minimum physical rate, not its nominal rate. */
    if ((uint64_t)maximum_cpu_hz * maximum_reference_ticks >=
        (UINT64_C(1) << 32) * minimum_reference_hz) {
        return false;
    }
    uint32_t cycles = cycle_end - cycle_start;
    uint32_t ticks = reference_end - reference_start;
    if (!cycles || !ticks || ticks > maximum_reference_ticks ||
        (uint64_t)cycles * minimum_reference_hz > (uint64_t)maximum_cpu_hz * ticks) {
        return false;
    }
    sample->cpu_cycles = cycles;
    sample->reference_ticks = ticks;
    return true;
}

/* Bounds must include the independently reviewed source accuracy, reference
 * accuracy and capture aperture. No tolerances or nominal silicon rates are
 * chosen here. Cross-products avoid rounded-Hz acceptance at a boundary. */
static inline bool nsx_clock_rate_within(
    nsx_clock_sample_t sample, uint32_t nominal_reference_hz,
    uint32_t lower_hz, uint32_t upper_hz)
{
    if (!sample.cpu_cycles || !sample.reference_ticks || !nominal_reference_hz ||
        !lower_hz || lower_hz > upper_hz) {
        return false;
    }
    uint64_t scaled_cycles = (uint64_t)sample.cpu_cycles * nominal_reference_hz;
    return scaled_cycles >= (uint64_t)lower_hz * sample.reference_ticks &&
           scaled_cycles <= (uint64_t)upper_hz * sample.reference_ticks;
}

#endif
