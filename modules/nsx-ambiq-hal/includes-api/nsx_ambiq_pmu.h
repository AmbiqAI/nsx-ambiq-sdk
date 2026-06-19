/**
 * @file nsx_ambiq_pmu.h
 * @brief Thin NSX-facing PMU shim over AmbiqSuite PMU utilities.
 */

#ifndef NSX_AMBIQ_PMU_H
#define NSX_AMBIQ_PMU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_AMBIQ_PMU_MAX_EVENT_COUNTERS 8

typedef struct {
    uint32_t counters;
    uint32_t event_types[NSX_AMBIQ_PMU_MAX_EVENT_COUNTERS];
} nsx_ambiq_pmu_config_t;

uint32_t nsx_ambiq_pmu_enable(void);
uint32_t nsx_ambiq_pmu_disable(void);
uint32_t nsx_ambiq_pmu_init(const nsx_ambiq_pmu_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif // NSX_AMBIQ_PMU_H
