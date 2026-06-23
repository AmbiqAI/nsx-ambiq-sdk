//*****************************************************************************
//
//! @file am_hal_clkgen_private.h
//!
//! @brief Internal API definition for Atomiq110 clock generator helpers.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************
//! @cond CLKGEN_PRIVATE_FUNC
#ifndef AM_HAL_CLKGEN_PRIVATE_H
#define AM_HAL_CLKGEN_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_hal_clkgen.h"

//
//! Default HFRC_ADJ that adjusts HFRC from 48MHz to 48MHz with 32.768kHz
//! XTAL_LS as reference clock.
//
#define AM_HAL_CLKGEN_DEFAULT_HFRC_ADJ_CONFIG                           \
    {                                                                   \
        .eRepeat        = AM_HAL_CLKGEN_HFRCADJ_RPT_4_SEC,              \
        .ui32TargetVal  = 0xEE6,                                        \
        .eWarmup        = AM_HAL_CLKGEN_HFRCAJD_XTWARMUP_1SEC,          \
        .eSpeed         = AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_2,    \
        .eMaxDelta      = AM_HAL_CLKGEN_HFRCADJ_MAXDELTA_DIS,           \
    }

//*****************************************************************************
//
//! @brief Calculate the HFRC adjust target value for a requested frequency.
//!
//! @param ui32RefFreq     XTAL_LS frequency in Hz.
//! @param ui32TargetFreq  HFRC frequency target in Hz.
//! @param pui32AdjTarget  Pointer that receives the computed adjust value.
//!
//! @return AM_HAL_STATUS_SUCCESS on success, error code otherwise.
//
//*****************************************************************************
extern uint32_t am_hal_clkgen_hfrcadj_target_calculate(uint32_t ui32RefFreq,
                                                       uint32_t ui32TargetFreq,
                                                       uint32_t *pui32AdjTarget);

//*****************************************************************************
//
//! @brief Configure and enable HFADJ.
//!
//! @param ui32RegVal  HFADJ register value to program (enable bit set inside).
//!
//! @return Execution status.
//
//*****************************************************************************
extern uint32_t am_hal_clkgen_private_hfadj_apply(uint32_t ui32RegVal);

//*****************************************************************************
//
//! @brief Disable HFADJ.
//!
//! @return Execution status.
//
//*****************************************************************************
extern uint32_t am_hal_clkgen_private_hfadj_disable(void);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_CLKGEN_PRIVATE_H
//! @endcond CLKGEN_PRIVATE_FUNC
