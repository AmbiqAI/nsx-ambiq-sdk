//*****************************************************************************
//
//! @file am_devices_xspi_nor_quirks.h
//!
//! @brief Quirk hooks for xSPI NOR flash devices.
//!
//! @addtogroup devices_xspi_nor_quirks xSPI NOR Quirks
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

#ifndef AM_DEVICES_XSPI_NOR_QUIRKS_H
#define AM_DEVICES_XSPI_NOR_QUIRKS_H

#include <stdint.h>
#include "am_devices_xspi.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @brief Flash chip specific non-JEDEC behavior hooks.
//
//*****************************************************************************
typedef struct am_devices_xspi_nor_quirks
{
    //! Optional replacement for default JEDEC soft reset sequence.
    uint32_t (*soft_reset)(am_hal_periph_handle_t *pFlash,
                           const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg);

    //! Called after reset is complete (default or device-specific).
    uint32_t (*post_reset)(am_hal_periph_handle_t *pFlash,
                           const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg);

    //! Called after common bootstrap init completes, before final mode switch.
    uint32_t (*post_init)(am_hal_periph_handle_t *pFlash);

    //! Optional replacement for JEDEC-ID transaction plus compact parse.
    uint32_t (*read_jedec_id)(am_hal_periph_handle_t *pFlash,
                              uint8_t *pRawId,
                              uint32_t ui32BufLen,
                              am_devices_xspi_info_t *pInfo);

    //! Called before common read flow.
    uint32_t (*pre_read)(am_hal_periph_handle_t *pFlash,
                         const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg,
                         uint32_t ui32Addr,
                         uint32_t ui32Size);

    //! Called after common read flow.
    uint32_t (*post_read)(am_hal_periph_handle_t *pFlash,
                          const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg,
                          uint32_t ui32Addr,
                          uint32_t ui32Size,
                          uint8_t *pData);

    //! Called before common write flow.
    uint32_t (*pre_write)(am_hal_periph_handle_t *pFlash,
                          const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg,
                          uint32_t ui32Addr,
                          uint32_t ui32Size,
                          const uint8_t *pData);

    //! Called after common write flow.
    uint32_t (*post_write)(am_hal_periph_handle_t *pFlash,
                           const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg,
                           uint32_t ui32Addr,
                           uint32_t ui32Size,
                           const uint8_t *pData);
} am_devices_xspi_nor_quirks_t;

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_QUIRKS_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
