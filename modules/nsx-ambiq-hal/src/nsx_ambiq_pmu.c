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

#if defined(AM_PART_ATOMIQ110)
/*
 * atomiq110_fpga_turbo bring-up: am_util_pmu_init() (vendored AmbiqSuite,
 * off-limits here) unconditionally calls am_hal_debug_enable() as its very
 * first step "to configure for general debug i.e. DBG power, tracing, TPIU,
 * etc.". On this FPGA build that call bus-faults -- it unconditionally
 * touches CRM_TPIUCLKCFG (CRM_BASE + 0x180), which is unreachable from
 * software on this part/board (see the matching fix and comment in
 * nsx-core's atomiq110/nsx_system_platform.c). PMU counter setup itself
 * does not need that debug/TPIU bring-up, so this reimplements
 * am_util_pmu_init()'s remaining steps -- ordinary ARMv8-M PMU/DCB/SCB
 * architectural registers, not Ambiq HAL/BSP -- skipping only the
 * am_hal_debug_enable() call that crashes.
 */
static uint32_t nsx_ambiq_pmu_init_atomiq110(am_util_pmu_config_t *cfg)
{
    uint32_t ui32IntMsk = 0;

    if (cfg == NULL) {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    // Enable Trace
    DCB->DEMCR |= DCB_DEMCR_MON_EN_Msk;

    // Enable Low Overhead Loops
    SCB->CCR |= SCB_CCR_LOB_Msk;

    // Configure Event Counters Registers
    for (uint32_t evtcnt = 0; evtcnt < __PMU_NUM_EVENTCNT; evtcnt++) {
        if (cfg->ui32Counters & (1U << evtcnt)) {
            if (evtcnt % 2 == 0 && cfg->ui32EventType[evtcnt] == ARM_PMU_CHAIN) {
                return AM_HAL_STATUS_INVALID_ARG;
            }
            ARM_PMU_Set_EVTYPER(evtcnt, cfg->ui32EventType[evtcnt]);
        }
    }

    // Reset Cycle Counter and Event Counters
    ARM_PMU_CYCCNT_Reset();
    ARM_PMU_EVCNTR_ALL_Reset();

    // Clears overflow status of event counter register.
    ARM_PMU_Set_CNTR_OVS(VALID_PMU_OVSCLRS);
    for (uint32_t evtcnt = 0; evtcnt < __PMU_NUM_EVENTCNT; evtcnt++) {
        if (evtcnt == __PMU_NUM_EVENTCNT - 1) {
            ui32IntMsk |= (1UL << evtcnt);
        } else if ((cfg->ui32EventType[evtcnt] == ARM_PMU_CHAIN) ||
                   (cfg->ui32EventType[evtcnt + 1] != ARM_PMU_CHAIN)) {
            ui32IntMsk |= (1UL << evtcnt);
        }
    }
    ui32IntMsk |= PMU_INTENSET_CCYCNT_ENABLE_Msk;
    ARM_PMU_Set_CNTR_IRQ_Enable(ui32IntMsk);

    // Enable Cycle Counter and Event Counters
    if (cfg->ui32Counters & (~VALID_PMU_COUNTERS)) {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }
    ARM_PMU_CNTR_Enable(cfg->ui32Counters);

    return AM_HAL_STATUS_SUCCESS;
}
#endif // AM_PART_ATOMIQ110

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

#if defined(AM_PART_ATOMIQ110)
    return nsx_ambiq_pmu_init_atomiq110(&sdk_cfg);
#else
    return am_util_pmu_init(&sdk_cfg);
#endif
}
