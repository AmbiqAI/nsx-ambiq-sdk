#ifndef NSX_POWER_INTERNAL_H
#define NSX_POWER_INTERNAL_H

#include <stddef.h>

#include "nsx_power.h"

/// One row of a platform staged power-profile register table.
typedef struct {
    volatile uint32_t *addr;
    const char *name;
    const char *description;
} nsx_power_profile_reg_t;

/**
 * @brief Return the platform staged power-profile register table.
 *
 * Implemented once per SoC arch directory. Platforms without staged
 * power-profile support return NULL (and set @p count to 0), which makes
 * nsx_power_profile_dump() report NSX_STATUS_INVALID_CONFIG.
 *
 * @param count Receives the number of rows in the returned table.
 * @return Pointer to the register table, or NULL when unsupported.
 */
const nsx_power_profile_reg_t *nsx_power_profile_platform_regs(size_t *count);

#endif
