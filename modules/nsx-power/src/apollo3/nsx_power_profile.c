/**
 * @brief Apollo3 family does not provide staged power-profile dumps.
 */

#include "nsx_power.h"

uint32_t nsx_power_profile_platform_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format) {
    (void)snapshot_index;
    (void)format;
    return NSX_STATUS_INVALID_CONFIG;
}
