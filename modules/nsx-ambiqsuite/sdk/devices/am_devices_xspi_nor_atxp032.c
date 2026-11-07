//*****************************************************************************
//
//! @file am_devices_xspi_nor_atxp032.c
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

#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor_atxp032.h"

#if AM_DEVICES_XSPI_NOR_ENABLE_ATXP032

#define ATXP032_CMD_ENTER_QUAD_MODE    0x38u
#define ATXP032_CMD_WRITE_STATUS_CTRL  0x71u
#define ATXP032_CMD_ENTER_OCTAL_MODE   0xE8u
#define ATXP032_CMD_RETURN_SPI_MODE    0xFFu

#define ATXP032_JEDEC_READ_LEN         11u

#define ATXP032_CR3_ADDR               0x03u
#define ATXP032_CR3_DUMMY_DEFAULT      0x05u
#define ATXP032_DUMMY_DEFAULT          18u

#define ATXP032_NOR_CMD(_cmd, _cmd4, _clen, _alen, _rxd, _txd)      \
        {                                                           \
            .ui16Cmd       = (_cmd),                                \
            .ui16Cmd4Byte  = (_cmd4),                               \
            .ui8CmdLen     = (_clen),                               \
            .ui8AddrLen    = (_alen),                               \
            .ui8RxDummy    = (_rxd),                                \
            .ui8TxDummy    = (_txd),                                \
            .ui8ModeBitLen = 0u,                                    \
            .ui32ModeBits  = 0u,                                    \
        }

static const am_devices_xspi_nor_erase_type_t g_sATXP032EraseTypes[] =
{
    { 64u * 1024u, AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K },
    { 32u * 1024u, AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K },
    { 4u * 1024u,  AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K  },
};

#define ATXP032_CMD_INFO_COMMON(_read_dummy, _rdsr_dummy, _read_id_cmd, _read_id_len) \
        [AM_DEVICES_XSPI_NOR_CMD_READ] =                                              \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_READ_FAST,                         \
                            AM_DEVICES_XSPI_NOR_OP_READ_FAST,                         \
                            1u, 4u, (_read_dummy), 0u),                               \
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =                                      \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_PP,                                \
                            AM_DEVICES_XSPI_NOR_OP_PP,                                \
                            1u, 4u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =                                       \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR,                              \
                            AM_DEVICES_XSPI_NOR_OP_RDSR,                              \
                            1u, 0u, (_rdsr_dummy), 0u),                               \
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =                                      \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR,                              \
                            AM_DEVICES_XSPI_NOR_OP_WRSR,                              \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =                                      \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_WREN,                              \
                            AM_DEVICES_XSPI_NOR_OP_WREN,                              \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =                                     \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_WRDI,                              \
                            AM_DEVICES_XSPI_NOR_OP_WRDI,                              \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =                                           \
            ATXP032_NOR_CMD((_read_id_cmd), (_read_id_cmd),                           \
                            (_read_id_len), 0u, 0u, 0u),                              \
        [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =                                         \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RDSFDP,                            \
                            AM_DEVICES_XSPI_NOR_OP_RDSFDP,                            \
                            1u, 3u, 8u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =                                   \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_SE,                                \
                            AM_DEVICES_XSPI_NOR_OP_SE,                                \
                            1u, 4u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =                                   \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_BE_32K,                            \
                            AM_DEVICES_XSPI_NOR_OP_BE_32K,                            \
                            1u, 4u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =                                   \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_BE,                                \
                            AM_DEVICES_XSPI_NOR_OP_BE,                                \
                            1u, 4u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =                                        \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_CE,                                \
                            AM_DEVICES_XSPI_NOR_OP_CE,                                \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =                                      \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_EN,                          \
                            AM_DEVICES_XSPI_NOR_OP_RESET_EN,                          \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =                                      \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_MEM,                         \
                            AM_DEVICES_XSPI_NOR_OP_RESET_MEM,                         \
                            1u, 0u, 0u, 0u)

#define ATXP032_CMD_INFO_ENTER_EXIT_4BYTE                                             \
        [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE] =                                       \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE,                       \
                            AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE,                       \
                            1u, 0u, 0u, 0u),                                          \
        [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE] =                                        \
            ATXP032_NOR_CMD(AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE,                        \
                            AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE,                        \
                            1u, 0u, 0u, 0u)

static const am_devices_xspi_nor_cmd_set_t g_sATXP032CmdSets[] =
{
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eIOMode   = AM_HAL_XSPIMC_IO_SERIAL,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            ATXP032_CMD_INFO_COMMON(8u,
                                    0u,
                                    AM_DEVICES_XSPI_NOR_OP_RDID,
                                    1u),
            ATXP032_CMD_INFO_ENTER_EXIT_4BYTE,
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .eIOMode   = AM_HAL_XSPIMC_IO_QUAD,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            ATXP032_CMD_INFO_COMMON(ATXP032_DUMMY_DEFAULT,
                                    4u,
                                    AM_DEVICES_XSPI_NOR_OP_NONE,
                                    0u),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
        .eIOMode   = AM_HAL_XSPIMC_IO_OCTAL,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            ATXP032_CMD_INFO_COMMON(ATXP032_DUMMY_DEFAULT,
                                    4u,
                                    AM_DEVICES_XSPI_NOR_OP_NONE,
                                    0u),
        },
    },
};

#undef ATXP032_CMD_INFO_ENTER_EXIT_4BYTE
#undef ATXP032_CMD_INFO_COMMON

#define ATXP032_JEDEC_CMD_INFO_COMMON(_read_dummy, _rdsr_dummy)                    \
        {                                                                          \
            .ui16ReadCmd        = AM_DEVICES_XSPI_NOR_OP_READ_FAST,                \
            .ui16PageProgramCmd = AM_DEVICES_XSPI_NOR_OP_PP,                       \
            .ui8ReadDummyCycles = (_read_dummy),                                   \
            .bUses4ByteAddr     = true,                                            \
            .bSFDPAddr4         = false,                                           \
            .bSFDPDummy20       = false,                                           \
            .bRDSRAddr4         = false,                                           \
            .ui8RDSRDummy       = (_rdsr_dummy),                                   \
            .bRDIDAddr4         = false,                                           \
            .ui8RDIDDummy       = 0u,                                              \
            .eDeviceModeDomain  = AM_DEVICES_XSPI_NOR_MODE_DOMAIN_AUTO,            \
            .eModeEnableReq     = AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE,            \
        }

#define ATXP032_JEDEC_PROFILE(_proto, _read_dummy, _rdsr_dummy)                    \
        {                                                                          \
            .eProtocol = (_proto),                                                 \
            .sCmdInfo  = ATXP032_JEDEC_CMD_INFO_COMMON(_read_dummy, _rdsr_dummy),  \
        }

static const am_devices_xspi_nor_jedec_cmd_profile_t g_sATXP032JEDECCmdProfiles[] =
{
    ATXP032_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S, 8, 0),
    ATXP032_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S, ATXP032_DUMMY_DEFAULT, 4),
    ATXP032_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S, ATXP032_DUMMY_DEFAULT, 4),
};

#undef ATXP032_JEDEC_PROFILE
#undef ATXP032_JEDEC_CMD_INFO_COMMON

static const uint8_t g_ui8ATXP032StatusCtrlCfg[]   = {ATXP032_CR3_DUMMY_DEFAULT};
static const uint8_t g_ui8ATXP032GlobalUnprotect[] = {0x00u};

#define ATXP032_WREN_SWITCH_OP                           \
        {                                                \
            .ui16Cmd      = AM_DEVICES_XSPI_NOR_OP_WREN, \
            .ui8CmdLen    = 1u,                          \
            .ui8AddrLen   = 0u,                          \
            .ui32Addr     = 0u,                          \
            .pui8Data     = NULL,                        \
            .ui32NumBytes = 0u,                          \
            .ui8TxDummy   = 0u,                          \
        }

static const am_devices_xspi_nor_switch_op_t g_sATXP032QuadOps[] =
{
    ATXP032_WREN_SWITCH_OP,
    {
        .ui16Cmd      = ATXP032_CMD_WRITE_STATUS_CTRL,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 1u,
        .ui32Addr     = ATXP032_CR3_ADDR,
        .pui8Data     = g_ui8ATXP032StatusCtrlCfg,
        .ui32NumBytes = sizeof(g_ui8ATXP032StatusCtrlCfg),
        .ui8TxDummy   = 0u,
    },
    ATXP032_WREN_SWITCH_OP,
    {
        .ui16Cmd      = ATXP032_CMD_ENTER_QUAD_MODE,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sATXP032OctalOps[] =
{
    ATXP032_WREN_SWITCH_OP,
    {
        .ui16Cmd      = ATXP032_CMD_WRITE_STATUS_CTRL,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 1u,
        .ui32Addr     = ATXP032_CR3_ADDR,
        .pui8Data     = g_ui8ATXP032StatusCtrlCfg,
        .ui32NumBytes = sizeof(g_ui8ATXP032StatusCtrlCfg),
        .ui8TxDummy   = 0u,
    },
    ATXP032_WREN_SWITCH_OP,
    {
        .ui16Cmd      = ATXP032_CMD_ENTER_OCTAL_MODE,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sATXP032SerialOps[] =
{
    ATXP032_WREN_SWITCH_OP,
    {
        .ui16Cmd      = ATXP032_CMD_RETURN_SPI_MODE,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_mode_switch_t g_sATXP032ModeSwitches[] =
{
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE,
        .pEnterOps       = NULL,
        .ui32NumEnterOps = 0u,
        .pExitOps        = NULL,
        .ui32NumExitOps  = 0u,
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        .pEnterOps       = g_sATXP032QuadOps,
        .ui32NumEnterOps = sizeof(g_sATXP032QuadOps) / sizeof(g_sATXP032QuadOps[0]),
        .pExitOps        = g_sATXP032SerialOps,
        .ui32NumExitOps  = sizeof(g_sATXP032SerialOps) / sizeof(g_sATXP032SerialOps[0]),
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        .pEnterOps       = g_sATXP032OctalOps,
        .ui32NumEnterOps = sizeof(g_sATXP032OctalOps) / sizeof(g_sATXP032OctalOps[0]),
        .pExitOps        = g_sATXP032SerialOps,
        .ui32NumExitOps  = sizeof(g_sATXP032SerialOps) / sizeof(g_sATXP032SerialOps[0]),
    },
};

static uint32_t atxp032_return_to_spi(am_hal_periph_handle_t *pFlash)
{
    if (am_devices_xspi_nor_write_enable(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          ATXP032_CMD_RETURN_SPI_MODE,
                                          1u,
                                          0u,
                                          0u,
                                          NULL,
                                          0u,
                                          0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t atxp032_control_freq_get(void)
{
    uint32_t ui32ControlFreq = g_sXSPINORATXP032.ui32ControlFreq;

    if (ui32ControlFreq == 0u)
    {
        ui32ControlFreq = g_sXSPINORATXP032.ui32DefaultFreq;
    }

    if ((ui32ControlFreq == 0u) ||
        (ui32ControlFreq > AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ))
    {
        ui32ControlFreq = AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ;
    }

    return ui32ControlFreq;
}

static uint32_t
atxp032_enter_mode(am_hal_periph_handle_t *pFlash,
                   am_devices_xspi_nor_protocol_e eProtocol)
{
    uint16_t ui16ModeCmd;

    switch (eProtocol)
    {
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S:
            return AM_DEVICES_XSPI_STATUS_SUCCESS;

        case AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S:
            ui16ModeCmd = ATXP032_CMD_ENTER_QUAD_MODE;
            break;

        case AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S:
            ui16ModeCmd = ATXP032_CMD_ENTER_OCTAL_MODE;
            break;

        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_write_enable(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          ui16ModeCmd,
                                          1u,
                                          0u,
                                          0u,
                                          NULL,
                                          0u,
                                          0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
atxp032_soft_reset(am_hal_periph_handle_t *pFlash,
                   const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg)
{
    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
                                           AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                           pProtocolCfg->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (atxp032_return_to_spi(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
                                           AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                           pProtocolCfg->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (atxp032_return_to_spi(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
                                           AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                           pProtocolCfg->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
atxp032_post_init(am_hal_periph_handle_t *pFlash)
{
    if (am_devices_xspi_nor_write_enable(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          AM_DEVICES_XSPI_NOR_OP_WRSR,
                                          1u,
                                          0u,
                                          0u,
                                          g_ui8ATXP032GlobalUnprotect,
                                          sizeof(g_ui8ATXP032GlobalUnprotect),
                                          0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_wait_ready(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

static uint32_t
atxp032_read_jedec_id(am_hal_periph_handle_t *pFlash,
                      uint8_t *pRawId,
                      uint32_t ui32BufLen,
                      am_devices_xspi_info_t *pInfo)
{
    am_devices_xspi_nor_protocol_cfg_t sSavedCfg;
    uint32_t                           ui32Status;
    uint32_t                           ui32RestoreStatus = AM_DEVICES_XSPI_STATUS_SUCCESS;
    uint32_t                           ui32ControlFreq;

    if (ui32BufLen < ATXP032_JEDEC_READ_LEN)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_protocol_cfg_get(pFlash, &sSavedCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32ControlFreq = atxp032_control_freq_get();

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           sSavedCfg.eProtocol,
                                           ui32ControlFreq,
                                           false))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((sSavedCfg.eProtocol != AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S) &&
        atxp032_return_to_spi(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
                                           ui32ControlFreq,
                                           false))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_devices_xspi_nor_command_read(pFlash,
                                                  AM_DEVICES_XSPI_NOR_OP_RDID,
                                                  1u,
                                                  0u,
                                                  0u,
                                                  pRawId,
                                                  ui32BufLen,
                                                  0u);
    if (ui32Status)
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
        goto atxp032_read_jedec_id_restore;
    }

    pInfo->ui8VendorId = pRawId[7];
    pInfo->ui8DeviceId = pRawId[8];
    ui32Status         = AM_DEVICES_XSPI_STATUS_SUCCESS;

atxp032_read_jedec_id_restore:
    if (sSavedCfg.eProtocol != AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S)
    {
        if (atxp032_enter_mode(pFlash, sSavedCfg.eProtocol))
        {
            ui32RestoreStatus = AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    if ((ui32RestoreStatus == AM_DEVICES_XSPI_STATUS_SUCCESS) &&
        am_devices_xspi_nor_apply_protocol(pFlash,
                                           sSavedCfg.eProtocol,
                                           sSavedCfg.ui32Freq,
                                           sSavedCfg.bDQS))
    {
        ui32RestoreStatus = AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((ui32Status != AM_DEVICES_XSPI_STATUS_SUCCESS) ||
        (ui32RestoreStatus != AM_DEVICES_XSPI_STATUS_SUCCESS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return ui32Status;
}

static const am_devices_xspi_nor_quirks_t g_sATXP032Quirks =
{
    .soft_reset    = atxp032_soft_reset,
    .post_reset    = NULL,
    .post_init     = atxp032_post_init,
    .read_jedec_id = atxp032_read_jedec_id,
    .pre_read      = NULL,
    .post_read     = NULL,
    .pre_write     = NULL,
    .post_write    = NULL,
};

const am_devices_xspi_nor_device_t g_sXSPINORATXP032 =
{
    .pName            = "ATXP032",
    .ui8VendorId      = 0x43u,
    .ui8DeviceId      = 0xA7u,
    .ui32DeviceSizeKb = AM_DEVICES_XSPI_NOR_ATXP032_FLASH_SIZE_KB,
    .ui32PageSize     = 256u,
    .ui32DefaultFreq  = AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
    .ui32ControlFreq  = AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
    .sCETiming        =
    {
        .ui32TCEMaxAct    = 0u,
        .ui32TCESetupHold = 2u,
        .ui32TCEMinInact  = 20u,
    },
    .eDefaultProtocol       = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
    .ui32SupportedProtoMask =
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S),
    .ui8JedecIdReadLen       = ATXP032_JEDEC_READ_LEN,
    .bSupport4ByteAddress    = true,
    .bUse4ByteOpcodes        = true,
    .eEnter4ByteMethod       = AM_DEVICES_XSPI_NOR_ENTER_4BYTE_DEDICATED_4B_OPCODES,
    .pEraseTypes             = g_sATXP032EraseTypes,
    .ui32NumEraseTypes       = sizeof(g_sATXP032EraseTypes) / sizeof(g_sATXP032EraseTypes[0]),
    .pCmdSets                = g_sATXP032CmdSets,
    .ui32NumCmdSets          = sizeof(g_sATXP032CmdSets) / sizeof(g_sATXP032CmdSets[0]),
    .pJEDECCmdProfiles       = g_sATXP032JEDECCmdProfiles,
    .ui32NumJEDECCmdProfiles = sizeof(g_sATXP032JEDECCmdProfiles) /
                               sizeof(g_sATXP032JEDECCmdProfiles[0]),
    .pModeSwitches           = g_sATXP032ModeSwitches,
    .ui32NumModeSwitches     = sizeof(g_sATXP032ModeSwitches) /
                               sizeof(g_sATXP032ModeSwitches[0]),
    .pQuirks = &g_sATXP032Quirks,
};

static uint32_t
atxp032_config(am_device_xspi_periph_handle_t *pFlash,
               const am_devices_xspi_norflash_cfg_t *pConfig)
{
    return am_devices_xspi_nor_config(pFlash, pConfig, &g_sXSPINORATXP032);
}

static uint32_t
atxp032_power(am_device_xspi_periph_handle_t *pFlash,
              am_devices_xspi_power_e ePowerMode,
              const am_devices_xspi_nor_power_cfg_t *pPowerCfg)
{
    return am_devices_xspi_nor_power_control(pFlash, ePowerMode, pPowerCfg);
}

const am_device_xspi_func_t apiATXP032 =
{
    .device_init   = am_devices_xspi_nor_init,
    .device_deinit = am_devices_xspi_nor_deinit,
    .device_config = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                   const am_devices_xspi_config_t *,
                                   ...)) atxp032_config,
    .device_get_info      = am_devices_xspi_nor_get_info,
    .device_write         = am_devices_xspi_nor_write,
    .device_read          = am_devices_xspi_nor_read,
    .device_erase         = am_devices_xspi_nor_erase,
    .device_set_xfer_mode = am_devices_xspi_nor_set_xfer_mode,
    .device_power_ctrl    = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) atxp032_power,
    .device_mem_map_config = am_devices_xspi_nor_mem_map_config,
    .device_timing_apply   = am_devices_xspi_nor_timing_apply,
};

#endif

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
