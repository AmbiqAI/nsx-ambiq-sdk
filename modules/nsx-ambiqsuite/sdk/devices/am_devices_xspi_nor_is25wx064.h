//*****************************************************************************
//
//! @file am_devices_xspi_nor_is25wx064.h
//!
//! @brief xSPI NOR wrapper for ISSI IS25WX064.
//!
//! @addtogroup devices_xspi_nor_is25wx064 IS25WX064 xSPI NOR
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

#ifndef AM_DEVICES_XSPI_NOR_IS25WX064_H
#define AM_DEVICES_XSPI_NOR_IS25WX064_H

#include "am_devices_xspi_nor.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064

#define AM_DEVICES_XSPI_NOR_IS25WX064_FLASH_SIZE_KB  (8u * 1024u)

extern const am_devices_xspi_nor_device_t g_sXSPINORIS25WX064;
extern const am_device_xspi_func_t apiIS25WX064;

#define AM_DEVICE_XSPI_NOR_IS25WX064_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                        \
                                         AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE,              \
                                         apiIS25WX064, XSPI_HANDLE)

#endif

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_IS25WX064_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
