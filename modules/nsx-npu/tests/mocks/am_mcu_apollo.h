/**
 * @file am_mcu_apollo.h
 * @brief Host-native mock of the tiny slice of ambiqsuite's Atomiq110
 *        CMSIS/HAL surface that `nsx_npu_timebase.c` depends on.
 *
 * This is NOT a copy of ambiqsuite. It exists only so nsx_npu_timebase.c can
 * be compiled and unit-tested on the host (no ARM cross-compiler, no
 * ambiqsuite checkout, no FPGA/silicon) with every clock input under the
 * test's direct control. Register bit positions/values and macro semantics
 * that matter to nsx_npu_timebase.c's logic are copied faithfully from
 * modules/nsx-ambiqsuite/sdk (see mock_hal.c's header comment for exact
 * sources); everything else is the minimum stand-in needed to satisfy the
 * compiler.
 *
 * Include-path trick: the test build points `-I` at this directory *before*
 * any real ambiqsuite include path, so `#include "am_mcu_apollo.h"` in
 * nsx_npu_timebase.c resolves here instead.
 */
#ifndef NSX_NPU_TESTS_MOCK_AM_MCU_APOLLO_H
#define NSX_NPU_TESTS_MOCK_AM_MCU_APOLLO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// FPGA marker + FPGA core-clock global (am_hal_global.h on real hardware).
//
// nsx_npu_timebase.c's self-calibration path is gated entirely behind this
// macro, so it must be defined for the host test to exercise that path --
// exactly as it unconditionally is inside ambiqsuite's real
// mcu/atomiq110/am_mcu_apollo.h today (see tools/nsx_npu_compile_check.py's
// notes: value 25 there, not that the value itself matters here).
//*****************************************************************************
#define ATOMIQ11X_FPGA (1)

extern uint32_t g_ui32FPGAfreqMHz;

//*****************************************************************************
// Bitfield helpers (core_ambiq_xtensa.h)
//*****************************************************************************
#define _VAL2FLD(field, value) (((uint32_t)(value) << field##_Pos) & field##_Msk)
#define _FLD2VAL(field, value) (((uint32_t)(value) & field##_Msk) >> field##_Pos)

//*****************************************************************************
// STIMER STCFG register (atomiq110.h) -- positions/masks/enum values copied
// verbatim from the real register map so the field packing this module
// relies on (CLKSEL in bits[3:0], CLEAR in bit30, FREEZE in bit31) is exact.
//*****************************************************************************
#define STIMER_STCFG_FREEZE_Pos (31UL)
#define STIMER_STCFG_FREEZE_Msk (0x80000000UL)
typedef enum
{
    STIMER_STCFG_FREEZE_THAW   = 0,
    STIMER_STCFG_FREEZE_FREEZE = 1,
} STIMER_STCFG_FREEZE_Enum;

#define STIMER_STCFG_CLEAR_Pos (30UL)
#define STIMER_STCFG_CLEAR_Msk (0x40000000UL)
typedef enum
{
    STIMER_STCFG_CLEAR_RUN   = 0,
    STIMER_STCFG_CLEAR_CLEAR = 1,
} STIMER_STCFG_CLEAR_Enum;

#define STIMER_STCFG_CLKSEL_Pos (0UL)
#define STIMER_STCFG_CLKSEL_Msk (0xfUL)
typedef enum
{
    STIMER_STCFG_CLKSEL_NOCLK          = 0,
    STIMER_STCFG_CLKSEL_HFRC_7_8125MHZ = 1,
    STIMER_STCFG_CLKSEL_HFRC_488_KHZ   = 2,
    STIMER_STCFG_CLKSEL_XTAL_32KHZ     = 3,
    STIMER_STCFG_CLKSEL_XTAL_8KHZ      = 4,
    STIMER_STCFG_CLKSEL_XTAL_2KHZ      = 5,
    STIMER_STCFG_CLKSEL_XTAL_1KHZ      = 6,
    STIMER_STCFG_CLKSEL_XTAL_512HZ     = 7,
    STIMER_STCFG_CLKSEL_LFRC_1KHZ      = 8,
    STIMER_STCFG_CLKSEL_CTIMER0        = 9,
    STIMER_STCFG_CLKSEL_CTIMER1        = 10,
    STIMER_STCFG_CLKSEL_HFRC_16MHZ     = 11,
} STIMER_STCFG_CLKSEL_Enum;

typedef struct
{
    volatile uint32_t STCFG;
} STIMER_Type;

extern STIMER_Type g_sMockStimer;
#define STIMER (&g_sMockStimer)

//*****************************************************************************
// am_hal_stimer.h
//*****************************************************************************
#define AM_HAL_STIMER_CFG_FREEZE _VAL2FLD(STIMER_STCFG_FREEZE, STIMER_STCFG_FREEZE_FREEZE)
#define AM_HAL_STIMER_CFG_RUN    _VAL2FLD(STIMER_STCFG_CLEAR, STIMER_STCFG_CLEAR_RUN)
#define AM_HAL_STIMER_HFRC_375KHZ _VAL2FLD(STIMER_STCFG_CLKSEL, STIMER_STCFG_CLKSEL_HFRC_488_KHZ)

extern uint32_t am_hal_stimer_config(uint32_t ui32STimerConfig);
extern bool     am_hal_stimer_is_running(void);
extern uint32_t am_hal_stimer_counter_get(void);
extern uint32_t am_hal_stimer_counter_clear(void);

//*****************************************************************************
// am_hal_status.h
//*****************************************************************************
#define AM_HAL_STATUS_SUCCESS (0U)

//*****************************************************************************
// am_hal_clkmgr.h
//*****************************************************************************
typedef enum
{
    AM_HAL_CLKMGR_CLK_ID_HFRC,
    AM_HAL_CLKMGR_CLK_ID_XTAL_LS,
} am_hal_clkmgr_clock_id_e;

typedef struct
{
    int iUnused; // Opaque to nsx_npu_timebase.c: it always passes NULL here.
} am_hal_clkmgr_clkcfg_t;

#define AM_HAL_CLKMGR_DEFAULT_XTAL_LS_FREQ_HZ (32768U)
#define AM_HAL_CLKMGR_HFRC_FREQ_ADJ_500MHZ    (500000000U)

extern uint32_t am_hal_clkmgr_clock_config_get(am_hal_clkmgr_clock_id_e eClockId,
                                                uint32_t *pui32RequestedClk,
                                                am_hal_clkmgr_clkcfg_t *psClockConfig);

//*****************************************************************************
// am_hal_utils.h
//*****************************************************************************
extern void am_hal_delay_us(uint32_t ui32us);

//*****************************************************************************
// am_hal_mcu_interrupt.h
//*****************************************************************************
extern uint32_t am_hal_interrupt_master_disable(void);
extern void     am_hal_interrupt_master_set(uint32_t ui32Level);

//*****************************************************************************
// CMSIS core (core_cm55.h): CPU cycle counter used only by the FPGA
// self-calibration path.
//*****************************************************************************
typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t CYCCNT;
} DWT_Type;

#define DWT_CTRL_CYCCNTENA_Msk (1UL)

extern DWT_Type g_sMockDwt;
#define DWT (&g_sMockDwt)

typedef struct
{
    volatile uint32_t DEMCR;
} CoreDebug_Type;

#define CoreDebug_DEMCR_TRCENA_Msk (0x01000000UL)

extern CoreDebug_Type g_sMockCoreDebug;
#define CoreDebug (&g_sMockCoreDebug)

#ifdef __cplusplus
}
#endif

#endif // NSX_NPU_TESTS_MOCK_AM_MCU_APOLLO_H
