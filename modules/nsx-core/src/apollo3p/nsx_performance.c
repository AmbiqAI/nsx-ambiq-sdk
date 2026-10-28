#include "am_mcu_apollo.h"
#include "nsx_performance_internal.h"

#if !APOLLO3_FPGA && !APOLLO3P_FPGA
static bool burst_initialized;

uint32_t nsx_performance_select(nsx_perf_mode_e mode) {
    if (mode != NSX_PERF_LOW && mode != NSX_PERF_MEDIUM && mode != NSX_PERF_HIGH)
        return NSX_STATUS_INVALID_CONFIG;

    bool high = mode != NSX_PERF_LOW;
    uint32_t divider = CLKGEN->CCTRL_b.CORESEL;
    if (divider > 1u || (high && divider != 0u))
        return NSX_STATUS_INVALID_CONFIG;

    if (high) {
        /* Startup owns supply preparation; the HAL owns silicon workarounds. */
        if (!MCUCTRL->SKU_b.ALLOWBURST || !PWRCTRL->SUPPLYSRC_b.BLEBUCKEN)
            return AM_HAL_STATUS_INVALID_OPERATION;
#if defined(AM_PART_APOLLO3P)
        /* AP3P initialization chooses its VDDF trim from this supply request. */
        if (PWRCTRL->MISC_b.SIMOBUCKEN != PWRCTRL->SUPPLYSTATUS_b.SIMOBUCKON)
            return AM_HAL_STATUS_INVALID_OPERATION;
#endif
        if (!burst_initialized) {
            am_hal_burst_avail_e available = AM_HAL_BURST_NOTAVAIL;
            uint32_t status = am_hal_burst_mode_initialize(&available);
            if (status != AM_HAL_STATUS_SUCCESS) return status;
            if (available != AM_HAL_BURST_AVAIL) return AM_HAL_STATUS_FAIL;
            burst_initialized = true;
        }
    }

    if (CLKGEN->FREQCTRL_b.BURSTREQ != (uint32_t)high ||
        CLKGEN->FREQCTRL_b.BURSTSTATUS != (uint32_t)high) {
        am_hal_burst_mode_e actual = high ? AM_HAL_NORMAL_MODE : AM_HAL_BURST_MODE;
        uint32_t status = high ? am_hal_burst_mode_enable(&actual) :
                                 am_hal_burst_mode_disable(&actual);
        if (status != AM_HAL_STATUS_SUCCESS) return status;
        if (actual != (high ? AM_HAL_BURST_MODE : AM_HAL_NORMAL_MODE))
            return AM_HAL_STATUS_FAIL;
    }
    if (CLKGEN->FREQCTRL_b.BURSTREQ != (uint32_t)high ||
        CLKGEN->FREQCTRL_b.BURSTSTATUS != (uint32_t)high ||
        CLKGEN->CCTRL_b.CORESEL != divider)
        return AM_HAL_STATUS_FAIL;

    SystemCoreClock = high ? 96000000u : (48000000u >> divider);
    return AM_HAL_STATUS_SUCCESS;
}
#else
uint32_t nsx_performance_select(nsx_perf_mode_e mode) {
    if (mode != NSX_PERF_LOW && mode != NSX_PERF_MEDIUM && mode != NSX_PERF_HIGH)
        return NSX_STATUS_INVALID_CONFIG;
    return AM_HAL_STATUS_INVALID_OPERATION;
}
#endif
