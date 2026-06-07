/**
 * @file nsx_pmu_map.h
 * @author Ambiq
 * @brief Convenient mapping of PMU registers to human-readable names
 * @version 0.1
 * @date 2024-08-15
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef NSX_PMU_MAP_H
#define NSX_PMU_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t eventId;
    const char regname[50];
    const char description[120];
} nsx_pmu_map_t;

#define NSX_PMU_EVENT_NAME_MAX_LEN 50

#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
extern uint32_t g_nsx_pmu_map_length;
#define NS_NUM_PMU_MAP_SIZE (g_nsx_pmu_map_length/sizeof(nsx_pmu_map_t))
extern const nsx_pmu_map_t nsx_pmu_map[];
#define NSX_PMU_MAP_SIZE 70
// (sizeof(nsx_pmu_map) / sizeof(nsx_pmu_map_t))
#else
#define NS_NUM_PMU_MAP_SIZE 0
#endif

#ifdef __cplusplus
}
#endif
#endif // NSX_PMU_MAP_H
