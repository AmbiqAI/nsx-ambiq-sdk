//*****************************************************************************
//
//! @file am_devices_display_rm69330.h
//!
//! @brief RM69330 display driver (MIPI-DSI command mode, QSPI/DSPI/SPI4).
//!
//! On MIPI-DSI, panel register traffic can use either:
//!   - DSI APB packet interface (no DC / DBIDSIBRIDGE), or
//!   - DSI Tx/Rx packet path (DC DBI -> DBIDSIBRIDGE -> DSI).
//! Frame pixels always use DC (and DBIDSIBRIDGE into DSI in command mode).
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
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_DEVICES_DISPLAY_RM69330_H
#define AM_DEVICES_DISPLAY_RM69330_H

#include "stdint.h"
#include "am_devices_display_types.h"

#ifndef DSI_IN_HS_MODE
//!
//! DSI link mode for panel register packets (DSI interfaces only).
//!   0 - Send commands in LP mode.
//!   1 - Send commands in HS mode.
//!
#define DSI_IN_HS_MODE              0
#endif

#ifndef DSI_APB_PACKET_INTERFACE
//!
//! Selects how panel register write/read packets are sent on DSI.
//!   0 - Tx/Rx packet path: DC DBI -> DBIDSIBRIDGE (DDB) -> DSI.
//!   1 - APB packet path: DSI host APB packet interface only (no DC/DDB).
//!
//! Frame pixels (WRITE_MEMORY_START / CONTINUE) always go through DC and
//! DBIDSIBRIDGE into DSI, regardless of this setting.
//!
#define DSI_APB_PACKET_INTERFACE    1
#endif

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
//! This function resets the display panel. Please execute it before function
//! am_devices_display_rm69330_init
//!
//
//*****************************************************************************
extern void am_devices_display_rm69330_hardware_reset(void);

//*****************************************************************************
//
//! @brief Get default DC / DDB / DSI / panel configuration for RM69330.
//!
//! For DSI, fills DC DBI timing, DBIDSIBRIDGE defaults, and DSI host/PHY
//! settings. For QSPI/DSPI/SPI4, fills the matching xSPI DC defaults.
//! Panel fields in `pDevCfg` mirror resolution, TE, format, and lane count.
//! Callers may override fields before HAL config and
//! `am_devices_display_rm69330_init()`.
//!
//! @param pDCCfg  - Optional; receives default DC configuration.
//! @param pDDBCfg - Optional; receives default DBIDSIBRIDGE configuration (DSI).
//! @param pDSICfg - Optional; receives default DSI configuration (DSI).
//! @param pDevCfg - Optional; receives default panel configuration.
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS`.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_get_default_config(am_hal_dc_config_t *pDCCfg,
                                                              am_hal_ddb_config_t *pDDBCfg,
                                                              am_hal_dsi_config_t *pDSICfg,
                                                              am_devices_dc_config_t *pDevCfg);
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
//! @return 32-bit status: `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success,
//!         otherwise an error status from the underlying DC transfer.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_flip(void *pHandle, uint8_t ui8FlipXY);

//*****************************************************************************
//
//! @brief Initialize RM69330 (and compatible) for SPI4/DSPI/QSPI or MIPI-DSI.
//!
//! Sends the panel bring-up command sequence. On DSI, register packets follow
//! `DSI_APB_PACKET_INTERFACE` (APB packet vs DC+DBIDSIBRIDGE Tx/Rx path).
//! Call after DC (and DSI/DDB when used) are powered and configured.
//!
//! @param ui32Module   - Module instance (typically 0).
//! @param ppHandle     - Out pointer that receives the device handle.
//! @param pDCHandle    - Display controller handle.
//! @param pDDBHandle   - DBIDSIBRIDGE handle (required for DSI Tx/Rx path and
//!                       for DSI frame pixels; may be unused for APB-only cmds).
//! @param pDSIHandle   - DSI handle (required when `g_sDispCfg` is DSI).
//! @param pDevCfg      - Panel configuration (resolution, format, window).
//!
//! @return `AM_DEVICES_DISPLAY_STATUS_SUCCESS` on success, otherwise an error.
//
//*****************************************************************************
extern uint32_t am_devices_display_rm69330_init(uint32_t ui32Module,
                                                void **ppHandle,
                                                void *pDCHandle,
                                                void *pDDBHandle,
                                                void *pDSIHandle,
                                                am_devices_dc_config_t *pDevCfg);

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

