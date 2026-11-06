//*****************************************************************************
//
//! @file am_devices_display_ls014b7dd01.h
//!
//! @brief Driver for SHARP display panel with JDI interface (LS014B7DD01).
//!
//! @addtogroup devices_display_ls014b7dd01 SHARP DC JDI Driver
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

#ifndef AM_DEVICES_DISPLAY_LS014B7DD01_H
#define AM_DEVICES_DISPLAY_LS014B7DD01_H

#include "stdint.h"
#include "am_devices_display_types.h"
#include "am_hal_timer.h"

#ifdef __cplusplus
extern "C"
{
#endif

//
// Timer settings used to generate JDI VCOM/VA PWM waveforms.
//
typedef struct
{
   uint32_t ui32TimerNum;              //!< Timer instance number.
   uint32_t ui32Frequency;             //!< PWM frequency in Hz.
   am_hal_timer_clock_e eTimerClock;   //!< Timer clock source.
}am_devices_display_timer_config_t;

//*****************************************************************************
//
//! External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Get default DC and panel configuration for LS014B7DD01.
//!
//! Fills `pDCCfg` with default JDI DC settings: JDI interface, fixed RGB222
//! destination format, 280 x 280 panel resolution, JDI timing, and target
//! clock rates. Fills `pDevCfg` with the default timer configuration used to
//! generate VCOM/VA PWM waveforms. Callers may override fields before
//! `am_hal_dc_config()` and `am_devices_display_ls014b7dd01_init()`.
//!
//! @param pDCCfg  - Pointer to receive the default DC configuration.
//! @param pDevCfg - Pointer to receive the default timer configuration.
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS`.
//
//*****************************************************************************
extern uint32_t am_devices_display_ls014b7dd01_get_default_config(am_hal_dc_config_t *pDCCfg,
                                                                  am_devices_display_timer_config_t *pDevCfg);
//*****************************************************************************
//
//! @brief Initialize the SHARP LS014B7DD01 panel (JDI interface).
//!
//! Configure the timer used to generate VCOM/PWM signals.
//!
//! @param ui32Module   - DC module number.
//! @param pDevCfg      - Pointer to an `am_devices_display_timer_config_t` that
//!                       specifies timer number, frequency and clock source for VCOM.
//! @param ppHandle     - Out pointer that receives the device handle.
//! @param pDCHandle    - Display controller handle.
//!
//! @return 32-bit status: `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_DISPLAY_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_display_ls014b7dd01_init(uint32_t ui32Module,
                                                    void **ppHandle, void *pDCHandle,
                                                    am_devices_display_timer_config_t *pDevCfg);

//*****************************************************************************
//
//! @brief Start the JDI VCOM/VA PWM timer.
//!
//! Enables the timer configured by `am_devices_display_ls014b7dd01_init()`.
//!
//! @param pHandle      - Device handle returned by
//!                       `am_devices_display_ls014b7dd01_init()`.
//!
//! @return `AM_HAL_STATUS_SUCCESS` on success.
//
//*****************************************************************************
extern uint32_t am_devices_display_ls014b7dd01_timer_enable(void *pHandle);

//*****************************************************************************
//
//! @brief Stop the JDI VCOM/VA PWM timer.
//!
//! Disables the timer configured by `am_devices_display_ls014b7dd01_init()`.
//!
//! @param pHandle      - Device handle returned by
//!                       `am_devices_display_ls014b7dd01_init()`.
//!
//! @return `AM_HAL_STATUS_SUCCESS` on success.
//
//*****************************************************************************
extern uint32_t am_devices_display_ls014b7dd01_timer_disable(void *pHandle);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_DISPLAY_LS014B7DD01_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

