//*****************************************************************************
//
//! @file am_devices_xspi_nor_mx25um51245g.h
//!
//! @brief xSPI NOR wrapper for Macronix MX25UM51245G.
//!
//! @addtogroup devices_xspi_nor_mx25um51245g MX25UM51245G xSPI NOR
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

#ifndef AM_DEVICES_XSPI_NOR_MX25UM51245G_H
#define AM_DEVICES_XSPI_NOR_MX25UM51245G_H

#include "am_devices_xspi_nor.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G

#define AM_DEVICES_XSPI_NOR_MX25UM51245G_FLASH_SIZE_KB  (64u * 1024u)

extern const am_devices_xspi_nor_device_t g_sXSPINORMX25UM51245G;
extern const am_device_xspi_func_t apiMX25UM51245G;

#define AM_DEVICE_XSPI_NOR_MX25UM51245G_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                           \
                                         AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE,                 \
                                         apiMX25UM51245G, XSPI_HANDLE)

#endif

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_MX25UM51245G_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
