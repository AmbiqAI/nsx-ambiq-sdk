//*****************************************************************************
//
//! @file am_devices_display_fl7703ni.h
//!
//! @brief FL7703NI display driver (MIPI-DSI video mode).
//!
//! Panel register bring-up uses the DSI APB packet interface (no DC /
//! DBIDSIBRIDGE). Continuous pixel refresh uses DC DPI into the DSI host.
//!
//! @addtogroup display_fl7703ni FL7703NI Display Device Driver
//! @ingroup devices
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_DEVICES_DISPLAY_FL7703NI_H
#define AM_DEVICES_DISPLAY_FL7703NI_H

#include "stdint.h"
#include "am_devices_display_types.h"

//!
//! DSI link mode for panel register packets during bring-up.
//!   0 - Send commands in LP mode.
//!   1 - Send commands in HS mode.
//!
#define DSI_IN_HS_MODE              0

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Assert / deassert the panel reset GPIO sequence.
//!
//! Call before `am_devices_display_fl7703ni_init()`.
//
//*****************************************************************************
extern void am_devices_display_fl7703ni_hardware_reset(void);

//*****************************************************************************
//
//! @brief Get default DC / DSI / panel configuration for FL7703NI.
//!
//! Fills DC DPI timing (640x480 defaults), matching DSI video-mode settings
//! (lanes, pixel format, video timing / BLLP), and panel fields in `pDevCfg`.
//! Callers may override before `am_hal_dc_config()`, `am_hal_dsi_config()`,
//! and `am_devices_display_fl7703ni_init()`.
//!
//! @param pDCCfg   - Optional; receives default DC DPI configuration.
//! @param pUnused  - Unused (kept for API symmetry with other panel drivers).
//! @param pDSICfg  - Optional; receives default DSI video-mode configuration.
//! @param pDevCfg  - Optional; receives default panel configuration.
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS`.
//
//*****************************************************************************
extern uint32_t am_devices_display_fl7703ni_get_default_config(am_hal_dc_config_t *pDCCfg,
                                                               am_hal_ddb_config_t *pUnused,
                                                               am_hal_dsi_config_t *pDSICfg,
                                                               am_devices_dc_config_t *pDevCfg);

//*****************************************************************************
//
//! @brief Initialize FL7703NI for DSI video mode.
//!
//! Sends manufacturer bring-up commands over the DSI APB packet interface.
//! Pixel streaming is driven later by DC DPI into DSI (configured by the
//! caller). Call after DC and DSI are powered and configured.
//!
//! @param ui32Module  - Module instance (typically 0).
//! @param ppHandle    - Out pointer that receives the device handle.
//! @param pDCHandle   - Display controller handle (stored; DPI path).
//! @param pUnused     - Unused (API symmetry with other panel drivers).
//! @param pDSIHandle  - DSI handle (APB packet commands).
//! @param pDevCfg     - Panel configuration.
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success, otherwise an error.
//
//*****************************************************************************
extern uint32_t am_devices_display_fl7703ni_init(uint32_t ui32Module,
                                                 void **ppHandle,
                                                 void *pDCHandle,
                                                 void *pUnused,
                                                 void *pDSIHandle,
                                                 am_devices_dc_config_t *pDevCfg);
#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_DISPLAY_FL7703NI_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

