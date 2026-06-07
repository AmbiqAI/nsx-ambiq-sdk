/**
 * @file nsx_ambiq_pmu.c
 * @brief Thin NSX-facing PMU shim over AmbiqSuite PMU utilities.
 */

#include "nsx_ambiq_pmu.h"

#include "am_mcu_apollo.h"
#include "am_util_pmu.h"

#include <string.h>

uint32_t nsx_ambiq_pmu_enable(void)
{
    return am_util_pmu_enable();
}

uint32_t nsx_ambiq_pmu_disable(void)
{
    return am_util_pmu_disable();
}

uint32_t nsx_ambiq_pmu_init(const nsx_ambiq_pmu_config_t *cfg)
{
    am_util_pmu_config_t sdk_cfg;

    if (cfg == NULL) {
        return 1U;
    }

    memset(&sdk_cfg, 0, sizeof(sdk_cfg));
    sdk_cfg.ui32Counters = cfg->counters;
    for (uint32_t index = 0; index < NSX_AMBIQ_PMU_MAX_EVENT_COUNTERS; ++index) {
        sdk_cfg.ui32EventType[index] = cfg->event_types[index];
    }

    return am_util_pmu_init(&sdk_cfg);
}
