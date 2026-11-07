//*****************************************************************************
//
//! @file am_devices_xspi_bridge.c
//!
//! @brief Atomiq110 xSPI bridge device driver for XSPIMC.
//
//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <string.h>
#include "am_bsp.h"
#include "am_devices_xspi_bridge.h"
#include "am_hal_utils.h"

#define XSPI_BRIDGE_XSPIMC_DUMMY_MAX 32u

typedef struct
{
    am_hal_periph_handle_t *pXSPIHandle;
    am_hal_xspimc_dev_id sDevice;
    am_hal_xspimc_dev_cfg_t sDeviceCfg;
    am_hal_xspimc_aip_cfg_t sAIPCfg;
    am_devices_xspi_timing_t sTimingCfg;
    am_hal_xspimc_xfer_mode_t eXferMode;
    uint32_t ui32MaxTxSize;
    uint32_t ui32MaxRxSize;
    bool bConfigured;
} am_devices_xspi_bridge_t;

typedef char xspi_bridge_state_size_check[
    (sizeof(am_devices_xspi_bridge_t) <= AM_DEVICES_XSPI_BRIDGE_GLOBAL_RAM_SIZE) ? 1 : -1];

typedef struct
{
    uint16_t ui16WriteInstr;
    uint16_t ui16ReadInstr;
    uint32_t ui32LimitSize;
} xspi_bridge_xspimc_cmd_limit_t;

static uint32_t
xspi_bridge_xspimc_cmd_limit_get(uint16_t ui16Instr,
                                 am_hal_xspimc_xfer_dir_e eDirection)
{
    static const xspi_bridge_xspimc_cmd_limit_t g_sCmdLimits[] =
    {
        {AM_DEVICES_XSPI_BRIDGE_WRITE_1BYTE, AM_DEVICES_XSPI_BRIDGE_READ_1BYTE, 1u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_2BYTES, AM_DEVICES_XSPI_BRIDGE_READ_2BYTES, 2u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL1, AM_DEVICES_XSPI_BRIDGE_READ_BL1, 4u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL2, AM_DEVICES_XSPI_BRIDGE_READ_BL2, 8u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL4, AM_DEVICES_XSPI_BRIDGE_READ_BL4, 16u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL8, AM_DEVICES_XSPI_BRIDGE_READ_BL8, 32u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL16, AM_DEVICES_XSPI_BRIDGE_READ_BL16, 64u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL32, AM_DEVICES_XSPI_BRIDGE_READ_BL32, 128u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL64, AM_DEVICES_XSPI_BRIDGE_READ_BL64, 256u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL128, AM_DEVICES_XSPI_BRIDGE_READ_BL128, 512u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_BL256, AM_DEVICES_XSPI_BRIDGE_READ_BL256, 1024u},
        {AM_DEVICES_XSPI_BRIDGE_WRITE_UL, AM_DEVICES_XSPI_BRIDGE_READ_UL, AM_DEVICES_XSPI_BRIDGE_MAX_PACKET_SIZE},
    };

    for (uint32_t i = 0u; i < (sizeof(g_sCmdLimits) / sizeof(g_sCmdLimits[0])); ++i)
    {
        if (((eDirection == AM_HAL_XSPIMC_XFER_READ) &&
             (ui16Instr == g_sCmdLimits[i].ui16ReadInstr)) ||
            ((eDirection == AM_HAL_XSPIMC_XFER_WRITE) &&
             (ui16Instr == g_sCmdLimits[i].ui16WriteInstr)))
        {
            return g_sCmdLimits[i].ui32LimitSize;
        }
    }

    return AM_DEVICES_XSPI_BRIDGE_MAX_PACKET_SIZE;
}

static uint32_t
xspi_bridge_xspimc_remote_mode_cmd_get(const am_hal_xspimc_dev_cfg_t *pDevCfg,
                                       uint32_t *pui32ModeCmd)
{
    if ((pDevCfg == NULL) || (pui32ModeCmd == NULL))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    switch (pDevCfg->eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_1_1_1;
            break;
        case AM_HAL_XSPIMC_IO_1_2_2:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_1_2_2;
            break;
        case AM_HAL_XSPIMC_IO_1_4_4:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_1_4_4;
            break;
        case AM_HAL_XSPIMC_IO_DUAL:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_2_2_2;
            break;
        case AM_HAL_XSPIMC_IO_QUAD:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_4_4_4;
            break;
        // #### INTERNAL BEGIN ####
        case AM_HAL_XSPIMC_IO_1_8_8:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_1_8_8;
            break;
        case AM_HAL_XSPIMC_IO_OCTAL:
            *pui32ModeCmd = AM_DEVICES_XSPI_BRIDGE_SOPM_8_8_8;
            break;
        // #### INTERNAL END ####
        default:
            return AM_HAL_STATUS_INVALID_ARG;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static void
xspi_bridge_xspimc_cov_devcfg(const am_devices_xspi_bridge_cfg_t *pCfg,
                              am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    *pDevCfg = (am_hal_xspimc_dev_cfg_t)
    {
        .ui32Freq = pCfg->ui32Freq,
        .eIOMode = pCfg->eIOMode,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .eCpp = pCfg->eCpp,
        .ui32ReadCmd = pCfg->ui32ReadCmd,
        .ui32WriteCmd = pCfg->ui32WriteCmd,
        .ui8CmdLen = 1u,
        .ui8AddrLen = 4u,
        .ui8AddrAlign = 1u,
        .ui8RxDummy = pCfg->ui8RxDummy,
        .ui8TxDummy = pCfg->ui8TxDummy,
        .sCETiming = pCfg->sCETiming,
        .eDeviceType = AM_HAL_XSPIMC_NONMEM_DEVICE,
    };
}

static uint32_t
xspi_bridge_xspimc_dev_cfg_validate(const am_hal_xspimc_dev_cfg_t *pDevCfg)
{
    if ((pDevCfg == NULL) ||
        (pDevCfg->ui32Freq == 0u) ||
        (pDevCfg->eDataRate != AM_HAL_XSPIMC_DATA_RATE_SINGLE) ||
        pDevCfg->bDQS ||
        ((pDevCfg->eCpp != AM_HAL_XSPIMC_CPP_MODE_0) &&
         (pDevCfg->eCpp != AM_HAL_XSPIMC_CPP_MODE_1)) ||
        (pDevCfg->ui8TxDummy > XSPI_BRIDGE_XSPIMC_DUMMY_MAX) ||
        (pDevCfg->ui8RxDummy > XSPI_BRIDGE_XSPIMC_DUMMY_MAX))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    if ((pDevCfg->ui8RxDummy < XSPI_BRIDGE_DUMMY_MIN) ||
        ((pDevCfg->eCpp == AM_HAL_XSPIMC_CPP_MODE_0) &&
         (pDevCfg->ui8RxDummy == XSPI_BRIDGE_DUMMY_MIN)))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static void
xspi_bridge_xspimc_cmd_limits_apply(am_devices_xspi_bridge_t *pDev,
                                    uint16_t ui16ReadInstr,
                                    uint16_t ui16WriteInstr)
{
    pDev->ui32MaxRxSize =
        xspi_bridge_xspimc_cmd_limit_get(ui16ReadInstr,
                                             AM_HAL_XSPIMC_XFER_READ);
    pDev->ui32MaxTxSize =
        xspi_bridge_xspimc_cmd_limit_get(ui16WriteInstr,
                                             AM_HAL_XSPIMC_XFER_WRITE);
}

static void
xspi_bridge_xspimc_serial_cfg_get(const am_hal_xspimc_dev_cfg_t *pSrcCfg,
                                  am_hal_xspimc_dev_cfg_t *pDstCfg)
{
    *pDstCfg = *pSrcCfg;
    pDstCfg->eIOMode = AM_HAL_XSPIMC_IO_SERIAL;
    pDstCfg->eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE;
    pDstCfg->ui8RxDummy = 0;
    pDstCfg->ui8TxDummy = 0;
}

static uint32_t
xspi_bridge_xspimc_min_cs_high_ns(uint32_t ui32RequestedFreq)
{
    // Minimum CS-high time is five SCLK periods, expressed in nanoseconds.
    uint64_t ui64Numerator = 5ull * 1000000000ull;

#if defined(ATOMIQ11X_FPGA)
    // Match the FPGA reference-clock scaling used by the XSPIMC HAL.
    ui64Numerator *= FPGA_FREQ_RATIO;
#endif

    return (uint32_t)((ui64Numerator + ui32RequestedFreq - 1u) /
                      ui32RequestedFreq);
}

static void
xspi_bridge_xspimc_protocol_timing_apply(am_hal_xspimc_dev_cfg_t *pCfg)
{
    uint32_t ui32MinCsHigh = xspi_bridge_xspimc_min_cs_high_ns(pCfg->ui32Freq);

    if (pCfg->sCETiming.ui32TCEMinInact < ui32MinCsHigh)
    {
        pCfg->sCETiming.ui32TCEMinInact = ui32MinCsHigh;
    }
}

static uint32_t
xspi_bridge_xspimc_timing_set(am_devices_xspi_bridge_t *pDev,
                              const am_devices_xspi_timing_t *pTiming)
{
    am_hal_xspimc_dev_cfg_t sSerialCfg;
    uint32_t ui32Status;

    xspi_bridge_xspimc_serial_cfg_get(&pDev->sDeviceCfg, &sSerialCfg);
    if (am_hal_xspimc_device_configure(pDev->pXSPIHandle, &pDev->sDevice,
                                       &sSerialCfg) != AM_HAL_STATUS_SUCCESS ||
        am_hal_xspimc_device_configure(pDev->pXSPIHandle, &pDev->sDevice,
                                       &pDev->sDeviceCfg) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_xspimc_control(pDev->pXSPIHandle, &pDev->sDevice,
                                        AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG, pTiming);
    if (ui32Status == AM_HAL_STATUS_INVALID_ARG || ui32Status == AM_HAL_STATUS_OUT_OF_RANGE)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    return ui32Status == AM_HAL_STATUS_SUCCESS ? AM_DEVICES_XSPI_STATUS_SUCCESS :
                                                AM_DEVICES_XSPI_STATUS_ERROR;
}

static uint32_t
xspi_bridge_xspimc_device_release(am_device_xspi_periph_handle_t *pHandle)
{
    am_devices_xspi_bridge_t *pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (am_hal_xspimc_disable(pDev->pXSPIHandle, &pDev->sDevice) != AM_HAL_STATUS_SUCCESS)
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
    }
    am_device_xspi_cpu_vote(pDev->pXSPIHandle->ui8ModuleNum, false);
    if (am_hal_periph_release(pHandle) != AM_HAL_STATUS_SUCCESS)
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return ui32Status;
}

static uint32_t
xspi_bridge_xspimc_device_acquire(am_device_xspi_periph_handle_t *pHandle)
{
    am_devices_xspi_bridge_t *pDev = (am_devices_xspi_bridge_t *)pHandle->pData;

    if (am_hal_periph_acquire(pHandle, AM_HAL_XSPIMC_TIMEOUT_MAX) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    am_device_xspi_cpu_vote(pDev->pXSPIHandle->ui8ModuleNum, true);
    if (am_hal_xspimc_enable(pDev->pXSPIHandle, &pDev->sDevice) != AM_HAL_STATUS_SUCCESS)
    {
        am_device_xspi_cpu_vote(pDev->pXSPIHandle->ui8ModuleNum, false);
        (void)am_hal_periph_release(pHandle);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pDev->bConfigured)
    {
        if (am_hal_xspimc_power_control(pDev->pXSPIHandle, &pDev->sDevice,
                                         AM_HAL_XSPIMC_POWER_REQUEST_ON) != AM_HAL_STATUS_SUCCESS ||
            xspi_bridge_xspimc_timing_set(pDev, &pDev->sTimingCfg) !=
                AM_DEVICES_XSPI_STATUS_SUCCESS)
        {
            (void)xspi_bridge_xspimc_device_release(pHandle);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        am_bsp_xspi_pins_enable(pDev->pXSPIHandle->ui8ModuleNum,
                                pDev->sDevice.ui8HWCENum, pDev->sDeviceCfg.eIOMode);
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
xspi_bridge_xspimc_transfer(am_devices_xspi_bridge_t *pDev,
                            am_hal_xspimc_xfer_mode_t eMode,
                            am_hal_xspimc_xfer_dir_e eDirection,
                            uint16_t ui16Cmd, bool bSendAddr, uint32_t ui32Addr,
                            void *pData, uint32_t ui32NumBytes)
{
    am_hal_xspimc_xfer_packet_t sPacket =
    {
        .eDir = eDirection,
        .ui16Cmd = ui16Cmd,
        .ui32Address = ui32Addr,
        .ui32NumBytes = ui32NumBytes,
        .pDataBuff = pData,
    };
    am_hal_xspimc_xfer_t sXfer =
    {
        .eXferMode = eMode,
        .ui8TxDummy = pDev->sDeviceCfg.ui8TxDummy,
        .ui8RxDummy = pDev->sDeviceCfg.ui8RxDummy,
        .ui8CmdLen = 1u,
        .ui8AddrLen = bSendAddr ? 4u : 0u,
        .pPackets = &sPacket,
        .ui32NumPacket = 1u,
        .ui32Timeout = AM_HAL_XSPIMC_TIMEOUT_MAX,
    };

    // XSPIMC0/1 support PIO only. DMA also requires word-aligned data.
    if (pDev->pXSPIHandle->ui8ModuleNum != 2u || ui32NumBytes == 0u ||
        (ui32NumBytes & 3u) != 0u || ((uintptr_t)pData & 3u) != 0u)
    {
        sXfer.eXferMode = AM_HAL_XSPIMC_PIO_XFER;
    }

    return am_hal_xspimc_xfer(pDev->pXSPIHandle, &pDev->sDevice, &sXfer) ==
               AM_HAL_STATUS_SUCCESS ? AM_DEVICES_XSPI_STATUS_SUCCESS :
                                       AM_DEVICES_XSPI_STATUS_ERROR;
}

static uint32_t
xspi_bridge_xspimc_command_write(am_devices_xspi_bridge_t *pDev,
                                 uint16_t ui16Cmd, bool bSendAddr, uint32_t ui32Addr,
                                 const void *pData, uint32_t ui32NumBytes)
{
    return xspi_bridge_xspimc_transfer(pDev, AM_HAL_XSPIMC_PIO_XFER,
                                       AM_HAL_XSPIMC_XFER_WRITE, ui16Cmd, bSendAddr,
                                       ui32Addr, (void *)pData, ui32NumBytes);
}

static uint32_t
xspi_bridge_xspimc_command_read(am_devices_xspi_bridge_t *pDev,
                                uint16_t ui16Cmd, bool bSendAddr, uint32_t ui32Addr,
                                void *pData, uint32_t ui32NumBytes)
{
    return xspi_bridge_xspimc_transfer(pDev, AM_HAL_XSPIMC_PIO_XFER,
                                       AM_HAL_XSPIMC_XFER_READ, ui16Cmd, bSendAddr,
                                       ui32Addr, pData, ui32NumBytes);
}

static uint32_t
xspi_bridge_xspimc_remote_reset(am_devices_xspi_bridge_t *pDev)
{
    uint32_t ui32Status = xspi_bridge_xspimc_command_write(pDev,
        AM_DEVICES_XSPI_BRIDGE_SOFT_RESET, false, 0u, NULL, 0u);

    am_hal_delay_us(1u);
    return ui32Status;
}

static uint32_t
am_devices_xspi_bridge_init(am_device_xspi_periph_handle_t *pHandle)
{
    am_devices_xspi_bridge_t *pDev;
    const am_devices_xspi_deps_t *pDeps;

    if (pHandle == NULL || pHandle->pData == NULL ||
        pHandle->uintSize < sizeof(*pDev) || pHandle->state.bInitialized)
    {
        return AM_HAL_STATUS_INVALID_HANDLE;
    }
    pDeps = (const am_devices_xspi_deps_t *)pHandle->pDeps;
    if (pDeps == NULL || pDeps->pXSPI == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    if (am_hal_periph_preinit(pHandle) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    memset(pDev, 0, sizeof(*pDev));
    pDev->pXSPIHandle = pDeps->pXSPI;
    pDev->sDevice.ui8HWCENum = pHandle->ui8ModuleNum;
    pDev->sDevice.eCEMode = AM_HAL_XSPIMC_HARDWARE_CE;
    pDev->eXferMode = AM_HAL_XSPIMC_PIO_XFER;
    pHandle->state.bInitialized = true;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
am_devices_xspi_bridge_config(am_device_xspi_periph_handle_t *pHandle,
                              const am_devices_xspi_config_t *pConfig, ...)
{
    am_devices_xspi_bridge_t *pDev;
    am_hal_xspimc_dev_cfg_t sDeviceCfg;
    const am_hal_xspimc_dev_cfg_t *pDeviceCfg = &sDeviceCfg;
    am_hal_xspimc_dev_cfg_t sSerialCfg;
    uint32_t ui32ModeCmd;
    uint32_t ui32Ver;
    uint32_t ui32Sts;
    uint32_t ui32Dummy;
    uint32_t ui32ActualFreq;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    if (pConfig == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    xspi_bridge_xspimc_cov_devcfg(pConfig, &sDeviceCfg);
    if (xspi_bridge_xspimc_dev_cfg_validate(pDeviceCfg) != AM_DEVICES_XSPI_STATUS_SUCCESS ||
        xspi_bridge_xspimc_remote_mode_cmd_get(pDeviceCfg, &ui32ModeCmd) !=
            AM_DEVICES_XSPI_STATUS_SUCCESS)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (xspi_bridge_xspimc_device_acquire(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if (am_hal_xspimc_power_control(pDev->pXSPIHandle, &pDev->sDevice,
                                     AM_HAL_XSPIMC_POWER_REQUEST_ON) != AM_HAL_STATUS_SUCCESS)
    {
        goto config_fail;
    }
    if (!pDev->bConfigured)
    {
        xspi_bridge_xspimc_serial_cfg_get(pDeviceCfg, &pDev->sDeviceCfg);
        xspi_bridge_xspimc_protocol_timing_apply(&pDev->sDeviceCfg);
        if (am_hal_xspimc_device_configure(pDev->pXSPIHandle, &pDev->sDevice,
                                           &pDev->sDeviceCfg) != AM_HAL_STATUS_SUCCESS)
        {
            goto config_fail;
        }
        am_bsp_xspi_pins_enable(pDev->pXSPIHandle->ui8ModuleNum,
                                pDev->sDevice.ui8HWCENum, AM_HAL_XSPIMC_IO_SERIAL);
    }
    if (xspi_bridge_xspimc_remote_reset(pDev))
    {
        goto config_fail;
    }

    pDev->sDeviceCfg = *pDeviceCfg;
    xspi_bridge_xspimc_protocol_timing_apply(&pDev->sDeviceCfg);
    xspi_bridge_xspimc_cmd_limits_apply(pDev, (uint16_t)pDeviceCfg->ui32ReadCmd,
                                        (uint16_t)pDeviceCfg->ui32WriteCmd);
    memset(&pDev->sTimingCfg, 0, sizeof(pDev->sTimingCfg));
    xspi_bridge_xspimc_serial_cfg_get(&pDev->sDeviceCfg, &sSerialCfg);
    if (am_hal_xspimc_device_configure(pDev->pXSPIHandle, &pDev->sDevice,
                                       &sSerialCfg) != AM_HAL_STATUS_SUCCESS)
    {
        goto config_fail;
    }
    ui32Dummy = pDeviceCfg->ui8RxDummy -
                  ((pDeviceCfg->eCpp == AM_HAL_XSPIMC_CPP_MODE_0) ? 1u : 0u);
    if (xspi_bridge_xspimc_command_write(pDev, AM_DEVICES_XSPI_BRIDGE_SET_DUMMY,
                                         false, 0u, &ui32Dummy, 1u) ||
        xspi_bridge_xspimc_command_write(pDev, (uint16_t)ui32ModeCmd,
                                         false, 0u, NULL, 0u))
    {
        goto config_fail;
    }
    ui32ActualFreq = pDeviceCfg->ui32Freq;
#if defined(ATOMIQ11X_FPGA)
    ui32ActualFreq /= FPGA_FREQ_RATIO;
#endif
    am_hal_delay_us((5000000u + ui32ActualFreq - 1u) / ui32ActualFreq);
    if (xspi_bridge_xspimc_timing_set(pDev, &pDev->sTimingCfg))
    {
        goto config_fail;
    }
    am_bsp_xspi_pins_enable(pDev->pXSPIHandle->ui8ModuleNum,
                            pDev->sDevice.ui8HWCENum, pDeviceCfg->eIOMode);
    if (xspi_bridge_xspimc_command_read(pDev, AM_DEVICES_XSPI_BRIDGE_READ_VER,
                                        false, 0u, &ui32Ver, sizeof(ui32Ver)) ||
        ui32Ver != AM_DEVICES_XSPI_BRIDGE_IP_VERSION ||
        xspi_bridge_xspimc_command_read(pDev, AM_DEVICES_XSPI_BRIDGE_READ_STS,
                                        false, 0u, &ui32Sts, sizeof(ui32Sts)) ||
        ((__REV(ui32Sts) & XSPI_BRIDGE_STS_SYNC_Msk) !=
         (XSPI_BRIDGE_STS_RESET_VALUE & XSPI_BRIDGE_STS_SYNC_Msk)) ||
        xspi_bridge_xspimc_command_write(pDev, AM_DEVICES_XSPI_BRIDGE_WRITE_STS,
                                         false, 0u, &ui32Sts, sizeof(ui32Sts)))
    {
        goto config_fail;
    }
    pDev->bConfigured = true;
    return xspi_bridge_xspimc_device_release(pHandle);

config_fail:
    pDev->bConfigured = false;
    (void)am_hal_xspimc_power_control(pDev->pXSPIHandle, &pDev->sDevice,
                                      AM_HAL_XSPIMC_POWER_REQUEST_OFF);
    (void)xspi_bridge_xspimc_device_release(pHandle);
    return AM_DEVICES_XSPI_STATUS_ERROR;
}

static uint32_t
am_devices_xspi_bridge_deinit(am_device_xspi_periph_handle_t *pHandle)
{
    am_devices_xspi_bridge_t *pDev;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (pDev->bConfigured)
    {
        if (xspi_bridge_xspimc_device_acquire(pHandle))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        pDev->sAIPCfg.bAIPEnable = false;
        for (uint32_t ui32Window = 0u;
             ui32Window < AM_HAL_XSPIMC_AIP_WINDOWS_PER_DEVICE; ++ui32Window)
        {
            pDev->sAIPCfg.ui8WindowNum = ui32Window;
            if (am_hal_xspimc_control(pDev->pXSPIHandle, &pDev->sDevice,
                                        AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                        &pDev->sAIPCfg) != AM_HAL_STATUS_SUCCESS)
            {
                (void)xspi_bridge_xspimc_device_release(pHandle);
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
        }
        if (xspi_bridge_xspimc_remote_reset(pDev) ||
            am_hal_xspimc_power_control(pDev->pXSPIHandle, &pDev->sDevice,
                                         AM_HAL_XSPIMC_POWER_REQUEST_OFF) != AM_HAL_STATUS_SUCCESS)
        {
            (void)xspi_bridge_xspimc_device_release(pHandle);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        if (xspi_bridge_xspimc_device_release(pHandle))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }
    if (am_hal_periph_postdeinit(pHandle) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pHandle->state.bInitialized = false;
    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
xspi_bridge_xspimc_read_write(am_device_xspi_periph_handle_t *pHandle,
                              am_hal_xspimc_xfer_dir_e eDirection,
                              uint32_t ui32Addr, uint32_t ui32NumBytes, void *pData)
{
    am_devices_xspi_bridge_t *pDev;
    uint8_t *pui8Data = (uint8_t *)pData;
    uint16_t ui16Cmd;
    uint32_t ui32Limit;
    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (!pDev->bConfigured || (pData == NULL && ui32NumBytes != 0u))
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    if (xspi_bridge_xspimc_device_acquire(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    ui16Cmd = (uint16_t)((eDirection == AM_HAL_XSPIMC_XFER_WRITE) ?
                         pDev->sDeviceCfg.ui32WriteCmd : pDev->sDeviceCfg.ui32ReadCmd);
    ui32Limit = (eDirection == AM_HAL_XSPIMC_XFER_WRITE) ? pDev->ui32MaxTxSize : pDev->ui32MaxRxSize;
    while (ui32NumBytes != 0u)
    {
        uint32_t ui32Chunk = ui32NumBytes > ui32Limit ? ui32Limit : ui32NumBytes;

        if (ui32Limit != AM_DEVICES_XSPI_BRIDGE_MAX_PACKET_SIZE && ui32Chunk < ui32Limit)
        {
            ui16Cmd = (eDirection == AM_HAL_XSPIMC_XFER_WRITE) ?
                          AM_DEVICES_XSPI_BRIDGE_WRITE_UL : AM_DEVICES_XSPI_BRIDGE_READ_UL;
        }
        ui32Status = xspi_bridge_xspimc_transfer(pDev, pDev->eXferMode, eDirection,
                                                ui16Cmd, true, ui32Addr, pui8Data, ui32Chunk);
        if (ui32Status)
        {
            break;
        }
        ui32Addr += ui32Chunk;
        pui8Data += ui32Chunk;
        ui32NumBytes -= ui32Chunk;
    }
    if (xspi_bridge_xspimc_device_release(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    return ui32Status;
}

static uint32_t
am_devices_xspi_bridge_write(am_device_xspi_periph_handle_t *pHandle,
                             uint32_t ui32Addr, uint32_t ui32NumBytes, void *pData)
{
    return xspi_bridge_xspimc_read_write(pHandle, AM_HAL_XSPIMC_XFER_WRITE,
                                         ui32Addr, ui32NumBytes, pData);
}

static uint32_t
am_devices_xspi_bridge_read(am_device_xspi_periph_handle_t *pHandle,
                            uint32_t ui32Addr, uint32_t ui32NumBytes, void *pData)
{
    return xspi_bridge_xspimc_read_write(pHandle, AM_HAL_XSPIMC_XFER_READ,
                                         ui32Addr, ui32NumBytes, pData);
}

static uint32_t
am_devices_xspi_bridge_set_xfer_mode(am_device_xspi_periph_handle_t *pHandle,
                                     am_hal_xspimc_xfer_mode_t eXferMode)
{
    am_devices_xspi_bridge_t *pDev;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    if (eXferMode != AM_HAL_XSPIMC_PIO_XFER && eXferMode != AM_HAL_XSPIMC_DMA_XFER)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    if (am_hal_periph_acquire(pHandle, AM_HAL_XSPIMC_TIMEOUT_MAX) != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    pDev->eXferMode = eXferMode;
    return am_hal_periph_release(pHandle) == AM_HAL_STATUS_SUCCESS ?
               AM_DEVICES_XSPI_STATUS_SUCCESS : AM_DEVICES_XSPI_STATUS_ERROR;
}

static uint32_t
am_devices_xspi_bridge_get_info(am_device_xspi_periph_handle_t *pHandle,
                                am_devices_xspi_info_t *pInfo)
{
    am_devices_xspi_bridge_t *pDev;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (!pDev->bConfigured || pInfo == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->ui32BaseAddr = pDev->sAIPCfg.ui32AIPBaseAddr;
    pInfo->sCE = pDev->sDevice;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
am_devices_xspi_bridge_mem_map_config(am_device_xspi_periph_handle_t *pHandle,
                                      const am_devices_xspi_memmap_cfg_t *pCfg)
{
    am_devices_xspi_bridge_t *pDev;
    am_hal_xspimc_aip_cfg_t sCfg;
    uint32_t ui32Status;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (!pDev->bConfigured || pCfg == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    if (xspi_bridge_xspimc_device_acquire(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sCfg = pDev->sAIPCfg;
    sCfg.ui32AIPBaseAddr = pCfg->ui32BaseAddr;
    sCfg.ui32MappedDevBaseOffset = pCfg->ui32MappedDevBaseOffset;
    sCfg.ui32AIPSize = pCfg->ui32Size;
    sCfg.ui8WindowNum = pCfg->ui8WindowNum;
    sCfg.ePermission = pCfg->ePermission;
    sCfg.bPrefetch = pCfg->bPrefetch;
    sCfg.bAIPEnable = pCfg->bEnable;
    ui32Status = am_hal_xspimc_control(pDev->pXSPIHandle, &pDev->sDevice,
                                        AM_HAL_XSPIMC_CONTROL_AIP_CONFIG, &sCfg);
    if (ui32Status == AM_HAL_STATUS_SUCCESS)
    {
        pDev->sAIPCfg = sCfg;
    }
    if (xspi_bridge_xspimc_device_release(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    return ui32Status == AM_HAL_STATUS_SUCCESS ? AM_DEVICES_XSPI_STATUS_SUCCESS :
                                                AM_DEVICES_XSPI_STATUS_ERROR;
}

static uint32_t
am_devices_xspi_bridge_timing_apply(am_device_xspi_periph_handle_t *pHandle,
                                    const am_devices_xspi_timing_t *pTiming)
{
    am_devices_xspi_bridge_t *pDev;
    uint32_t ui32Status;

    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)
    pDev = (am_devices_xspi_bridge_t *)pHandle->pData;
    if (!pDev->bConfigured || pTiming == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    if (xspi_bridge_xspimc_device_acquire(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    ui32Status = xspi_bridge_xspimc_timing_set(pDev, pTiming);
    if (ui32Status == AM_DEVICES_XSPI_STATUS_SUCCESS)
    {
        pDev->sTimingCfg = *pTiming;
    }
    if (xspi_bridge_xspimc_device_release(pHandle))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    return ui32Status;
}

const am_device_xspi_func_t apiXSPIBridge =
{
    .device_init           = am_devices_xspi_bridge_init,
    .device_deinit         = am_devices_xspi_bridge_deinit,
    .device_config         = am_devices_xspi_bridge_config,
    .device_get_info       = am_devices_xspi_bridge_get_info,
    .device_write          = am_devices_xspi_bridge_write,
    .device_read           = am_devices_xspi_bridge_read,
    .device_erase          = NULL,
    .device_set_xfer_mode  = am_devices_xspi_bridge_set_xfer_mode,
    .device_power_ctrl     = NULL,
    .device_mem_map_config = am_devices_xspi_bridge_mem_map_config,
    .device_timing_apply   = am_devices_xspi_bridge_timing_apply,
};
