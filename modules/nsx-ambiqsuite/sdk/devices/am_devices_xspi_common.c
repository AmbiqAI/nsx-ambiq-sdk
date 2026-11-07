//*****************************************************************************
//
//! @file am_devices_xspi_common.h
//!
//! @brief General/common part of drivers for xSPI devices
//!
//! @addtogroup devices_xspi_common xSPI Driver Common
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
#include "am_devices_xspi.h"
#include "am_mcu_apollo.h"

// Timing scan

void am_device_xspi_cpu_vote(uint32_t ui32Xspi, bool bOn)
{
    if (bOn)
    {
        am_hal_pwrctrl_mem_vote_request(AM_HAL_PWRCTRL_VOTE_MASTER_CPU_AXI, AM_HAL_PWRCTRL_VOTE_MEMORY_XSPI0 + ui32Xspi);
    }
    else
    {
        am_hal_pwrctrl_mem_vote_release(AM_HAL_PWRCTRL_VOTE_MASTER_CPU_AXI, AM_HAL_PWRCTRL_VOTE_MEMORY_XSPI0 + ui32Xspi);
    }
}

uint32_t
am_device_xspi_bus_init(am_hal_periph_handle_t *pController, const am_hal_xspimc_cfg_t *pCfg)
{
    uint32_t ui32Status = am_hal_xspimc_initialize(pController);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, true);

    ui32Status = am_hal_xspimc_power_control(pController, NULL, AM_HAL_XSPIMC_POWER_REQUEST_ON);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_xspimc_cfg_t sCfg =
    {
        .pDMACmdList        = NULL,
        .ui32DMACmdListSize = 0,
        .ui8DMABurstLen     = 0,
        .ui8FIFOLevel       = 7,
        .bDMAIRQALO         = false,
    };

    if (pCfg)
    {
        sCfg = *pCfg;
    }

    ui32Status = am_hal_xspimc_configure(pController, NULL, &sCfg);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_device_xspi_bus_deinit(am_hal_periph_handle_t *pController)
{
    uint32_t ui32Status;

    ui32Status = am_hal_xspimc_power_control(pController, NULL, AM_HAL_XSPIMC_POWER_REQUEST_OFF);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_xspimc_deinitialize(pController);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
