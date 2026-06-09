/**
 * @brief Common power-profile wrapper.
 */

#include "nsx_power.h"

extern uint32_t nsx_power_profile_platform_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format);

uint32_t nsx_power_profile_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format) {
    if ((format != NSX_POWER_PROFILE_FORMAT_JSON) &&
        (format != NSX_POWER_PROFILE_FORMAT_CSV)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    return nsx_power_profile_platform_dump(snapshot_index, format);
}
