#ifndef NSX_POWER_BUCK_INTERNAL_H
#define NSX_POWER_BUCK_INTERNAL_H

#include <stdbool.h>
#include "nsx_power_buck.h"

/* Selected by the build. All hardware calls run under the common session lock.
 * check must be read-only. start sets touched when stop is needed, even on error. */
nsx_buck_status_t nsx_buck_hw_check(nsx_buck_rail_t rail, uint8_t timer);
nsx_buck_status_t nsx_buck_hw_start(nsx_buck_rail_t rail, uint8_t timer, bool *touched);
nsx_buck_status_t nsx_buck_hw_stop(uint8_t timer);
/* Write the count only if the timer is still enabled locally and globally. */
bool nsx_buck_hw_count(uint8_t timer, uint32_t *count);
/* Return nominal STIMER Hz or zero for an unusable or unrecognized timebase. */
uint32_t nsx_buck_hw_time(uint32_t *tick);
nsx_buck_modes_t nsx_buck_hw_modes(void);

#endif
