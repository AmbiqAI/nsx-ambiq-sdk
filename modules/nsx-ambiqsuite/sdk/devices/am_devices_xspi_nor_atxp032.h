//*****************************************************************************
//
//! @file am_devices_xspi_nor_atxp032.h
//!
//! @brief xSPI NOR wrapper for Adesto ATXP032.
//!
//! @addtogroup devices_xspi_nor_atxp032 ATXP032 xSPI NOR
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

#ifndef AM_DEVICES_XSPI_NOR_ATXP032_H
#define AM_DEVICES_XSPI_NOR_ATXP032_H

#include "am_devices_xspi_nor.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if AM_DEVICES_XSPI_NOR_ENABLE_ATXP032

#define AM_DEVICES_XSPI_NOR_ATXP032_FLASH_SIZE_KB   (4u * 1024u)

extern const am_devices_xspi_nor_device_t g_sXSPINORATXP032;
extern const am_device_xspi_func_t apiATXP032;

#define AM_DEVICE_XSPI_NOR_ATXP032_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                      \
                                         AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE,            \
                                         apiATXP032, XSPI_HANDLE)

#endif

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_ATXP032_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
