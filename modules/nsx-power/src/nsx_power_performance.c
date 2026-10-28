/** Map the legacy power enum explicitly into the core-owned selector. */
#include "nsx_power.h"
#include "nsx_performance_internal.h"

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    switch (mode) {
#if !defined(AM_PART_APOLLO3) && !defined(AM_PART_APOLLO3P) && \
    !defined(AM_PART_APOLLO4P) && !defined(AM_PART_APOLLO4L)
    case NSX_POWER_PERF_NOT_SET:
#endif
    case NSX_POWER_PERF_LOW: return nsx_performance_select(NSX_PERF_LOW);
    case NSX_POWER_PERF_HIGH: return nsx_performance_select(NSX_PERF_MEDIUM);
    case NSX_POWER_PERF_MAX: return nsx_performance_select(NSX_PERF_HIGH);
    default: return NSX_STATUS_INVALID_CONFIG;
    }
}
