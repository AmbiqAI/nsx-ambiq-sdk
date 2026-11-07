//*****************************************************************************
//
//! @file am_devices_xspi_nor_cmds.h
//!
//! @brief Generic command tables for xSPI NOR protocols.
//!
//! @addtogroup devices_xspi_nor_cmds xSPI NOR Command Tables
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

#ifndef AM_DEVICES_XSPI_NOR_CMDS_H
#define AM_DEVICES_XSPI_NOR_CMDS_H

#include "am_devices_xspi_nor.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern const am_devices_xspi_nor_cmd_set_t *
am_devices_xspi_nor_cmd_set_get(am_devices_xspi_nor_protocol_e eProtocol);

extern const am_devices_xspi_nor_jedec_cmd_profile_t *
am_devices_xspi_nor_jedec_cmd_profile_get(am_devices_xspi_nor_protocol_e eProtocol);

extern am_devices_xspi_nor_protocol_e
am_devices_xspi_nor_protocol_from_hal(am_hal_xspimc_io_mode_e eIOMode,
                                      am_hal_xspimc_data_rate_e eDataRate);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_CMDS_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
