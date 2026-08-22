/**
 * @brief Apollo3 family does not provide staged power-profile dumps.
 */

#include "nsx_power_internal.h"

const nsx_power_profile_reg_t *nsx_power_profile_platform_regs(size_t *count) {
    *count = 0u;
    return NULL;
}
