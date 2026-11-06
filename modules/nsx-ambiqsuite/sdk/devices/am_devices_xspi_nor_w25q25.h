//*****************************************************************************
//
//! @file am_devices_xspi_nor_w25q25.h
//!
//! @brief xSPI NOR wrapper for Winbond W25Q25PW_DTR.
//!
//! @addtogroup devices_xspi_nor_w25q25 W25Q25PW_DTR xSPI NOR
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

#ifndef AM_DEVICES_XSPI_NOR_W25Q25_H
#define AM_DEVICES_XSPI_NOR_W25Q25_H

#include "am_devices_xspi_nor.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if AM_DEVICES_XSPI_NOR_ENABLE_W25Q25

#define AM_DEVICES_XSPI_NOR_W25Q25_FLASH_SIZE_KB  (32u * 1024u)

//! W25Q25PW_DTR device descriptor.
extern const am_devices_xspi_nor_device_t g_sXSPINORW25Q25;

//! W25Q25PW_DTR device API.
extern const am_device_xspi_func_t apiW25Q25;

#define AM_DEVICE_XSPI_NOR_W25Q25_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                     \
                                         AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE,           \
                                         apiW25Q25, XSPI_HANDLE)

#endif

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_W25Q25_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
