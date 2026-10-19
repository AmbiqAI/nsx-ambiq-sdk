/** CPU performance selection for Apollo510 and Apollo330P/510L. */
#include "am_mcu_apollo.h"
#include "am_hal_clkmgr.h"
#include "nsx_power.h"

uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    am_hal_pwrctrl_mcu_mode_e hardware_mode;
    uint32_t core_hz;

#if defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    if (mode == NSX_POWER_PERF_MAX) {
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
    } else if (mode == NSX_POWER_PERF_HIGH) {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1;
        core_hz = AM_HAL_CLKGEN_FREQ_HP192_HZ;
    } else {
        hardware_mode = AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER;
        core_hz = AM_HAL_CLKGEN_FREQ_MAX_HZ;
    }
#else
    if (mode == NSX_POWER_PERF_HIGH || mode == NSX_POWER_PERF_MAX) {
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
    if (status == AM_HAL_STATUS_SUCCESS) {
        // The caller owns SysTick and other timebases; updating this CMSIS
        // value does not reprogram their reload values or cached conversions.
        SystemCoreClock = core_hz;
    }
    return status;
}
