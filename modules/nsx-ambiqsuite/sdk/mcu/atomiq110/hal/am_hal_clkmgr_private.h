//*****************************************************************************
//
//! @file am_hal_clkmgr_private.h
//!
//! @brief Clock manager functions that manage system clocks and minimize
//!        power consumption by powering down clocks when possible.
//!
//! @addtogroup clkmgr_at110 CLKMGR - Clock Manager
//! @ingroup atomiq110_hal
//! @{
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
//! @cond CLKMGR_PRIVATE_FUNC
#ifndef AM_HAL_CLKMGR_PRIVATE_H
#define AM_HAL_CLKMGR_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_hal_clkmgr.h"

//*****************************************************************************
//
//! @brief Clock manager handling before CPU enters deepsleep
//!
//! @note This API is inteded for use by HAL only. Do not call this API from
//!       Application/BSP.
//! @note This API is to be called from the critical section of the deep sleep
//!       handling
//
//*****************************************************************************
extern void am_hal_clkmgr_private_deepsleep_enter();

//*****************************************************************************
//
//! @brief Clock manager handling after CPU exits from deepsleep
//!
//! @note This API is inteded for use by HAL only. Do not call this API from
//!       Application/BSP.
//! @note This API is to be called from the critical section of the deep sleep
//!       handling
//
//*****************************************************************************
extern void am_hal_clkmgr_private_deepsleep_exit();

#ifdef __cplusplus
}
#endif

#endif //AM_HAL_CLKMGR_PRIVATE_H
//! @endcond CLKMGR_PRIVATE_FUNC

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
