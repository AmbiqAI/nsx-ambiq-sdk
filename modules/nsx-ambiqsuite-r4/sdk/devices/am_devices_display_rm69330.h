//*****************************************************************************
//
//! @file am_devices_display_rm69330.h
//!
//! @brief RM69330 display driver interfaces( MIPI-DSI, xSPI).
//!
//! @addtogroup display_rm69330 RM69330 Display Device Driver
//! @ingroup devices
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.17 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_DEVICES_DISPLAY_RM69330_H
#define AM_DEVICES_DISPLAY_RM69330_H

#include "stdint.h"
#include "am_devices_display_types.h"

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
//! @brief Reset the display panel
//!
//! This function resets the display panel.
//!
//! @param pHandle - Device handle (unused).
//
//*****************************************************************************
extern void am_devices_display_rm69330_hardware_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Return default NemaDC xSPI configuration for RM69330.
//!
//! Populates interface type, RGB888 destination format, panel resolution,
//! SPI mode, and target read/write clock rates from BSP settings.
//!
//! @param pHandle - Device handle (unused).
//! @param pDCCfg  - Pointer to receive the DC configuration.
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS`.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_get_parameter(void *pHandle,
                                                        am_hal_dc_config_t *pDCCfg);
//*****************************************************************************
//
//! @brief Set the flip along with the x/y-axis or both the x and y-axis.
//!
//! @param pHandle              - Device handle returned by
//!                               `am_devices_display_rm69330_init()`.
//! @param ui8FlipXY            - how to flip the display
//!
//! @note Register bits function for Driver IC RM69330 or CO5300.
//!       Bitfield 0 - Reserved                 (please don't set this bit)
//!       Bitfield 1 - Flip along with y-axis   (reserved for CO5300)
//!       Bitfield 2 - Reserved
//!       Bitfield 3 - RGB or BGR order
//!       Bitfield 4 - Flip along with x-axis   (reserved for CO5300)
//!       Bitfield 5 - Flip along y = x         (reserved for CO5300)
//!       Bitfield 6 - Flip along with y-axis
//!       Bitfield 7 - Flip along with x-axis
//! The x-axis will keep no flip when setting bitfields 4 and 7 same time. In the
//! same way, the same is true for the y-axis. Please be careful some of the flips
//! could cause the tear effect.
//!
//! @return None.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_flip(void *pHandle, uint8_t ui8FlipXY);

//*****************************************************************************
//
//! @brief Initialize Raydium panels (RM67162, RM69090, RM69330) using DC xSPI.
//!
//! Configures the display controller and panel according to `pDevCfg`.
//! Call after NemaDC is initialized and configured.
//!
//! @param ui32Module           - DC module number.
//! @param pDevCfg              - Pointer to an `am_devices_dc_config_t`
//!                               containing desired panel/DC configuration.
//! @param ppHandle             - Out pointer that receives the device handle.
//! @param pDCHandle            - Display controller handle.
//!
//! @return 32-bit status: `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_DISPLAY_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_init(uint32_t ui32Module,
                                         am_devices_dc_config_t *pDevCfg,
                                         void **ppHandle, void *pDCHandle);

//*****************************************************************************
//
//! @brief Set the display scanline index.
//!
//! Updates the current scanline index used for frame timing.
//!
//! @param pHandle      - Device handle returned by
//!                       `am_devices_display_rm69330_init()`.
//! @param ui16ScanLine - Scanline index to set.
//! @param ui16ResY     - Vertical resolution (height) of the display in pixels.
//!
//! @note Returns an error if `ui16ScanLine` is greater than or equal to
//!       the display's vertical resolution (`ui16ResY`).
//!
//! @return 32-bit status: `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_DISPLAY_STATUS_OUT_OF_RANGE` if `ui16ScanLine` invalid,
//!         otherwise `AM_DEVICES_DISPLAY_STATUS_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_set_scanline(void *pHandle,
                                                        uint16_t ui16ScanLine,
                                                        uint16_t ui16ResY);

//*****************************************************************************
//
//! @brief Set the region of the panel to be updated.
//!
//! Configures the panel's column/row window for subsequent frame transfers.
//!
//! @param pHandle   - Device handle returned by
//!                    `am_devices_display_rm69330_init()`.
//! @param ui16ResX  - Width of the region in pixels.
//! @param ui16ResY  - Height of the region in pixels.
//! @param ui16MinX  - X coordinate of the region origin (left).
//! @param ui16MinY  - Y coordinate of the region origin (top).
//!
//! @return 32-bit status: `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_DISPLAY_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_set_region(void *pHandle,
                                                      uint16_t ui16ResX,
                                                      uint16_t ui16ResY,
                                                      uint16_t ui16MinX,
                                                      uint16_t ui16MinY);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_DISPLAY_RM69330_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

