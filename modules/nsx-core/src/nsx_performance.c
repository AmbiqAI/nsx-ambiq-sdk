#include "am_mcu_apollo.h"
#include "nsx_performance_internal.h"

#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
uint32_t nsx_performance_select(nsx_perf_mode_e mode) {
    if (mode != NSX_PERF_LOW && mode != NSX_PERF_MEDIUM && mode != NSX_PERF_HIGH)
        return NSX_STATUS_INVALID_CONFIG;
    bool high = mode != NSX_PERF_LOW;
    am_hal_pwrctrl_mcu_mode_e hardware_mode = high ?
        AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE : AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER;
    uint32_t status = am_hal_pwrctrl_mcu_mode_select(hardware_mode);
    if (status != AM_HAL_STATUS_SUCCESS) return status;
    /* The HAL's cached mode can precede a failed hardware handshake. */
    if (!PWRCTRL->MCUPERFREQ_b.MCUPERFACK ||
        PWRCTRL->MCUPERFREQ_b.MCUPERFSTATUS != (uint32_t)hardware_mode)
        return AM_HAL_STATUS_FAIL;
#if !defined(APOLLO4_FPGA)
    SystemCoreClock = AM_HAL_CLKGEN_FREQ_MAX_HZ * (high ? 2u : 1u);
#endif
    return AM_HAL_STATUS_SUCCESS;
}
#else
#include "am_hal_clkmgr.h"
uint32_t nsx_performance_select(nsx_perf_mode_e mode) {
    if (mode != NSX_PERF_LOW && mode != NSX_PERF_MEDIUM && mode != NSX_PERF_HIGH)
        return NSX_STATUS_INVALID_CONFIG;
    am_hal_pwrctrl_mcu_mode_e hardware_mode;
    uint32_t core_hz;

#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    if (mode == NSX_PERF_HIGH) {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2;
#if defined(APOLLO5_FPGA)
        // The FPGA runs scaled clocks, while CLKMGR retains nominal silicon
        // PLL configuration values for the HAL's own mode-switch validation.
        core_hz = AM_HAL_CLKGEN_FREQ_HP250_HZ;
#else
        // HP2 runs from PLLVCO. Audio can configure a rate below 250 MHz;
        // do not publish the nominal maximum as the current CPU frequency.
        uint32_t status = am_hal_clkmgr_clock_config_get(
            AM_HAL_CLKMGR_CLK_ID_PLLVCO, &core_hz, NULL);
        if (status != AM_HAL_STATUS_SUCCESS) {
            return status;
        }
        if (core_hz < AM_HAL_CLKGEN_FREQ_HP192_HZ || core_hz > AM_HAL_CLKGEN_FREQ_HP250_HZ) {
            return AM_HAL_STATUS_INVALID_ARG;
        }
#endif
    } else if (mode == NSX_PERF_MEDIUM) {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1;
        core_hz = AM_HAL_CLKGEN_FREQ_HP192_HZ;
    } else {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER;
        core_hz = AM_HAL_CLKGEN_FREQ_MAX_HZ;
    }
#else
    if (mode == NSX_PERF_MEDIUM || mode == NSX_PERF_HIGH) {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE;
#if defined(APOLLO5_FPGA)
        core_hz = AM_HAL_CLKGEN_FREQ_HP250_HZ;
#else
        // Audio can configure the shared HP CPU source to 196.608 MHz.
        // Merely entering HP mode does not restore it to 250 MHz.
        uint32_t status = am_hal_clkmgr_clock_config_get(
            AM_HAL_CLKMGR_CLK_ID_HFRC2, &core_hz, NULL);
        if (status != AM_HAL_STATUS_SUCCESS) {
            return status;
        }
        // CLKMGR represents nominal free-running HFRC2 by a sentinel (0),
        // not a frequency in hertz. Normalize only its supported settings.
        if (core_hz == AM_HAL_CLKMGR_HFRC2_FREQ_FREE_RUN_APPROX_250MHZ) {
            core_hz = AM_HAL_CLKGEN_FREQ_HP250_HZ;
        } else if (core_hz != AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_250MHZ &&
                   core_hz != AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_196P608MHZ) {
            return AM_HAL_STATUS_INVALID_ARG;
        }
        // Configuration can be deferred until an audio/peripheral user
        // requests HFRC2. The CPU mode switch does not apply HF2ADJ itself.
        // A pending adjusted setting is not the free-running CPU clock.
#if defined(AM_PART_APOLLO510)
        // Audio adjustment can retune the shared HP CPU oscillator. Its
        // enable bit does not prove lock or qualify another CPU frequency.
        if (CLKGEN->HF2ADJ0_b.HF2ADJEN == CLKGEN_HF2ADJ0_HF2ADJEN_EN &&
            core_hz == AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_196P608MHZ) {
            return AM_HAL_STATUS_INVALID_OPERATION;
        }
#endif
        if (CLKGEN->HF2ADJ0_b.HF2ADJEN != CLKGEN_HF2ADJ0_HF2ADJEN_EN) {
            core_hz = AM_HAL_CLKGEN_FREQ_HP250_HZ;
        }
#endif
    } else {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER;
        core_hz = AM_HAL_CLKGEN_FREQ_MAX_HZ;
    }
#endif

    uint32_t status = am_hal_pwrctrl_mcu_mode_select(hardware_mode);
    if (status == AM_HAL_STATUS_SUCCESS &&
        (!PWRCTRL->MCUPERFREQ_b.MCUPERFACK ||
         PWRCTRL->MCUPERFREQ_b.MCUPERFSTATUS != (uint32_t)hardware_mode)) {
        return AM_HAL_STATUS_FAIL;
    }
    if (status == AM_HAL_STATUS_SUCCESS) {
        // The caller owns SysTick and other timebases; updating this CMSIS
        // value does not reprogram their reload values or cached conversions.
        SystemCoreClock = core_hz;
    }
    return status;
}

#endif
