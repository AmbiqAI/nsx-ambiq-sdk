/**
 * @file mock_hal.c
 * @brief Simulation model backing mock_hal.h, and the definitions of the
 *        HAL/CMSIS functions and globals declared in mocks/am_mcu_apollo.h.
 *
 * Model: two independent free-running counters (STIMER and DWT->CYCCNT), each
 * with a test-configurable rate in Hz. mock_advance_us() -- called by every
 * am_hal_delay_us() a test lets nsx_npu_timebase.c perform -- advances both
 * by rate_hz * elapsed_us / 1e6, carrying the division remainder across calls
 * (the same exact-rational-conversion technique nsx_npu_timebase.c itself
 * uses for its virtual clock) so no fractional tick is silently dropped over
 * many small advances.
 *
 * Register field values are copied from ambiqsuite's real Atomiq110 map (see
 * mocks/am_mcu_apollo.h's header comment); everything here is otherwise a
 * from-scratch simulation, not a copy of any ambiqsuite HAL source file.
 */
#include "am_mcu_apollo.h"
#include "mock_hal.h"

STIMER_Type    g_sMockStimer     = { 0 };
DWT_Type       g_sMockDwt        = { 0 };
CoreDebug_Type g_sMockCoreDebug  = { 0 };
uint32_t       g_ui32FPGAfreqMHz = 25U;

static bool     s_bHalRunning;
static uint32_t s_ui32StimerHz;
static uint32_t s_ui32CyccntHz;
static uint64_t s_ui64StimerRem;
static uint64_t s_ui64CyccntRem;

typedef struct
{
    uint32_t ui32Status;
    uint32_t ui32Hz;
} mock_clkmgr_entry_t;

static mock_clkmgr_entry_t s_asClkMgr[2];

// Private shadow "COUNTER" register: real STIMER exposes it as a separate
// register from STCFG; am_hal_stimer_counter_get() is the only accessor
// nsx_npu_timebase.c uses, so that is all this model needs to back.
static uint32_t s_ui32StimerCounter;

void mock_hal_reset(void)
{
    g_sMockStimer.STCFG    = STIMER_STCFG_CLKSEL_NOCLK;
    g_sMockDwt.CTRL        = 0U;
    g_sMockDwt.CYCCNT      = 0U;
    g_sMockCoreDebug.DEMCR = 0U;
    g_ui32FPGAfreqMHz      = 25U;

    s_bHalRunning  = false;
    s_ui32StimerHz = 0U;
    s_ui32CyccntHz = 0U;
    s_ui64StimerRem = 0U;
    s_ui64CyccntRem = 0U;

    // The real, observed atomiq110_fpga_turbo bug this module's calibration
    // path exists for: CLKMGR reports the clock as configured but unable to
    // supply a rate.
    s_asClkMgr[AM_HAL_CLKMGR_CLK_ID_HFRC].ui32Status    = AM_HAL_STATUS_SUCCESS;
    s_asClkMgr[AM_HAL_CLKMGR_CLK_ID_HFRC].ui32Hz        = 0U;
    s_asClkMgr[AM_HAL_CLKMGR_CLK_ID_XTAL_LS].ui32Status = AM_HAL_STATUS_SUCCESS;
    s_asClkMgr[AM_HAL_CLKMGR_CLK_ID_XTAL_LS].ui32Hz     = 0U;

    s_ui32StimerCounter = 0U;
}

void mock_stimer_set_counter(uint32_t ui32Value)
{
    s_ui32StimerCounter = ui32Value;
}

void mock_stimer_set_hal_running(bool bRunning)
{
    s_bHalRunning = bRunning;
}

void mock_set_rates_hz(uint32_t ui32StimerHz, uint32_t ui32CyccntHz)
{
    s_ui32StimerHz = ui32StimerHz;
    s_ui32CyccntHz = ui32CyccntHz;
}

void mock_advance_us(uint32_t ui32Us)
{
    if (s_ui32StimerHz != 0U)
    {
        const uint64_t ui64Num = ((uint64_t)s_ui32StimerHz * ui32Us) + s_ui64StimerRem;
        s_ui32StimerCounter += (uint32_t)(ui64Num / 1000000U);
        s_ui64StimerRem = ui64Num % 1000000U;
    }

    // Real DWT only counts when CoreDebug->DEMCR's TRCENA bit *and*
    // DWT->CTRL's CYCCNTENA bit are both set -- nsx_npu_timebase.c sets both
    // itself before reading CYCCNT, but the mock still honours the gate so a
    // test can exercise "trace never actually took effect" as its own
    // distinct failure mode from "CPU clock rate is 0".
    const bool bCyccntEnabled =
        ((g_sMockCoreDebug.DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
        ((g_sMockDwt.CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U);

    if (bCyccntEnabled && (s_ui32CyccntHz != 0U))
    {
        const uint64_t ui64Num = ((uint64_t)s_ui32CyccntHz * ui32Us) + s_ui64CyccntRem;
        g_sMockDwt.CYCCNT += (uint32_t)(ui64Num / 1000000U);
        s_ui64CyccntRem = ui64Num % 1000000U;
    }
}

void mock_set_clkmgr(am_hal_clkmgr_clock_id_e eClockId, uint32_t ui32Status, uint32_t ui32Hz)
{
    s_asClkMgr[eClockId].ui32Status = ui32Status;
    s_asClkMgr[eClockId].ui32Hz     = ui32Hz;
}

void mock_set_fpga_freq_mhz(uint32_t ui32Mhz)
{
    g_ui32FPGAfreqMHz = ui32Mhz;
}

//*****************************************************************************
// HAL function definitions
//*****************************************************************************

uint32_t am_hal_stimer_config(uint32_t ui32STimerConfig)
{
    g_sMockStimer.STCFG = ui32STimerConfig;
    s_bHalRunning        = true; // matches real HAL: config() marks it HAL-owned/running.
    return AM_HAL_STATUS_SUCCESS;
}

bool am_hal_stimer_is_running(void)
{
    return s_bHalRunning;
}

uint32_t am_hal_stimer_counter_get(void)
{
    return s_ui32StimerCounter;
}

uint32_t am_hal_stimer_counter_clear(void)
{
    s_ui32StimerCounter = 0U;
    return AM_HAL_STATUS_SUCCESS;
}

uint32_t am_hal_clkmgr_clock_config_get(am_hal_clkmgr_clock_id_e eClockId,
                                        uint32_t *pui32RequestedClk,
                                        am_hal_clkmgr_clkcfg_t *psClockConfig)
{
    (void)psClockConfig;
    *pui32RequestedClk = s_asClkMgr[eClockId].ui32Hz;
    return s_asClkMgr[eClockId].ui32Status;
}

void am_hal_delay_us(uint32_t ui32us)
{
    mock_advance_us(ui32us);
}

uint32_t am_hal_interrupt_master_disable(void)
{
    return 0U;
}

void am_hal_interrupt_master_set(uint32_t ui32Level)
{
    (void)ui32Level;
}
