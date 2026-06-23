//*****************************************************************************
//
//! @file am_hal_mcuctrl_private.h
//!
//! @brief Internal api definition for internal mcuctrl functions
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
//! @cond MCUCTRL_PRIVATE_FUNC
#ifndef AM_HAL_MCUCTRL_PRIVATE_H
#define AM_HAL_MCUCTRL_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_hal_mcuctrl.h"

// Default trim values for XTAL_HS
#define XTALHSCAP2TRIM_DEFAULT  44
#define XTALHSCAPTRIM_DEFAULT   4

// ****************************************************************************
//
//! @brief Enable XTAL_HS in kickstart mode.
//!
//! @note This API is intended for use by HAL only. Do not call this API from
//!       Application/BSP.
//!
//! @param bExternalClock - TRUE: configure for external clock mode,
//!                        FALSE: configure for crystal mode.
//!
//! @return status  - Execution status as defined in am_hal_status_e.
//!                   Possible return values:
//!                   - AM_HAL_STATUS_SUCCESS: XTAL_HS successfully configured
//!                     and enabled in the requested mode.
//!                   - AM_HAL_STATUS_INVALID_OPERATION: XTAL_HS is already
//!                     configured in a different mode (XTAL vs EXT) than
//!                     requested. The hardware cannot switch modes without
//!                     first being turned off.
//
// ****************************************************************************
extern uint32_t am_hal_mcuctrl_private_xtalhs_kickstart(bool bExternalClock);

// ****************************************************************************
//
//! @brief Enable XTAL_HS in normal mode.
//!
//! @note This API is intended for use by HAL only. Do not call this API from
//!       Application/BSP.
//!
//! @param bExternalClock - TRUE: configure for external clock mode,
//!                        FALSE: configure for crystal mode.
//!
//! @return status  - Execution status as defined in am_hal_status_e.
//!                   Possible return values:
//!                   - AM_HAL_STATUS_SUCCESS: XTAL_HS successfully configured
//!                     and enabled in the requested mode.
//!                   - AM_HAL_STATUS_INVALID_OPERATION: XTAL_HS is already
//!                     configured in a different mode (XTAL vs EXT) than
//!                     requested. The hardware cannot switch modes without
//!                     first being turned off.
//
// ****************************************************************************
extern uint32_t am_hal_mcuctrl_private_xtalhs_normal(bool bExternalClock);

// ****************************************************************************
//
//! @brief Disable XTAL_HS (turn off).
//!
//! @note This API is intended for use by HAL only. Do not call this API from
//!       Application/BSP.
//!
//! @return status  - Execution status as defined in am_hal_status_e.
//!                   Possible return values:
//!                   - AM_HAL_STATUS_SUCCESS: XTAL_HS successfully powered down
//!                     and disabled, or was already off (no changes made).
//
// ****************************************************************************
extern uint32_t am_hal_mcuctrl_private_xtalhs_off(void);

// ****************************************************************************
//
//! @brief Enable XTAL_LS (EXTCLK32K) in XTAL or external clock mode.
//!
//! @note This API is intended for use by HAL only. Do not call this API from
//!       Application/BSP.
//!
//! @param bExternalClock - TRUE: configure for external clock mode,
//!                        FALSE: configure for crystal mode.
//!
//! @return status  - Execution status as defined in am_hal_status_e.
//
// ****************************************************************************
extern uint32_t am_hal_mcuctrl_private_extclk32k_enable(bool bExternalClock);

// ****************************************************************************
//
//! @brief Disable XTAL_LS (EXTCLK32K).
//!
//! @note This API is intended for use by HAL only. Do not call this API from
//!       Application/BSP.
//!
//! @return status  - Execution status as defined in am_hal_status_e.
//
// ****************************************************************************
extern uint32_t am_hal_mcuctrl_private_extclk32k_disable(void);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_MCUCTRL_PRIVATE_H
//! @endcond MCUCTRL_PRIVATE_FUNC
