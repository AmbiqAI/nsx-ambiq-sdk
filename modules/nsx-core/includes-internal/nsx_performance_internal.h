#ifndef NSX_PERFORMANCE_INTERNAL_H
#define NSX_PERFORMANCE_INTERNAL_H
#include "nsx_system.h"

/* Shared foreground operation; caller owns HAL baseline and timing reloads. */
uint32_t nsx_performance_select(nsx_perf_mode_e mode);
#endif
