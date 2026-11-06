//*****************************************************************************
//
//! @file am_devices_xspi_nor_is25wx064.c
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

#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor_is25wx064.h"

#if AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064

#define IS25WX064_CMD_WR_VCFGREG          AM_DEVICES_XSPI_NOR_OP_WR_VCFGREG
#define IS25WX064_CMD_RD_VCFGREG          AM_DEVICES_XSPI_NOR_OP_RD_VCFGREG
#define IS25WX064_CMD_READ_FLAG_STATUS    AM_DEVICES_XSPI_NOR_OP_RDFLSR
#define IS25WX064_CR_REG_ADDR_IO_MODE     0x00u
#define IS25WX064_CR_IO_EXT_SPI           0xFFu
#define IS25WX064_CR_IO_OCTAL_DDR         0xE7u

#define IS25WX064_OCMD_DTR_READ           \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_1_8_8_READ_DTR_4B)
#define IS25WX064_OCMD_PP                 \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_PP_4B)
#define IS25WX064_OCMD_RDSR               \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RDSR)
#define IS25WX064_OCMD_WRSR               \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_WRSR)
#define IS25WX064_OCMD_WREN               \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_WREN)
#define IS25WX064_OCMD_WRDI               \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_WRDI)
#define IS25WX064_OCMD_RDID               \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RDID)
#define IS25WX064_OCMD_RDSFDP             \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RDSFDP)
#define IS25WX064_OCMD_SE_4K              \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_SE_4B)
#define IS25WX064_OCMD_BE_32K             \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_BE_32K_4B)
#define IS25WX064_OCMD_BE_128K             \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_BE_4B)
#define IS25WX064_OCMD_CE                 \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_CE_ALT)
#define IS25WX064_OCMD_RESET_ENABLE       \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RESET_EN)
#define IS25WX064_OCMD_RESET_MEMORY       \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RESET_MEM)
#define IS25WX064_OCMD_RD_VCFGREG         \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(IS25WX064_CMD_RD_VCFGREG)
#define IS25WX064_OCMD_WR_VCFGREG         \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(IS25WX064_CMD_WR_VCFGREG)
#define IS25WX064_OCMD_SUSPEND            \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_SUSPEND)
#define IS25WX064_OCMD_RESUME             \
        AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(AM_DEVICES_XSPI_NOR_OP_RESUME)

#define IS25WX064_CMD_BLOCK_ERASE_128K   AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K //enum name inaccurate

#define IS25_NOR_CMD(_cmd, _clen, _alen, _rxd, _txd)          \
        {                                                     \
            .ui16Cmd       = (_cmd),                          \
            .ui16Cmd4Byte  = (_cmd),                          \
            .ui8CmdLen     = (_clen),                         \
            .ui8AddrLen    = (_alen),                         \
            .ui8RxDummy    = (_rxd),                          \
            .ui8TxDummy    = (_txd),                          \
            .ui8ModeBitLen = 0u,                              \
            .ui32ModeBits  = 0u,                              \
        }

static const am_devices_xspi_nor_erase_type_t g_sIS25WX064EraseTypes[] =
{
    { 4u * 1024u,   AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K  },
    { 32u * 1024u,  AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K  },
    { 128u * 1024u, IS25WX064_CMD_BLOCK_ERASE_128K },
};

static const am_devices_xspi_nor_cmd_set_t g_sIS25WX064CmdSets[] =
{
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .eIOMode   = AM_HAL_XSPIMC_IO_OCTAL,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_DUAL,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                IS25_NOR_CMD(IS25WX064_OCMD_DTR_READ, 2u, 4u, 16u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
                IS25_NOR_CMD(IS25WX064_OCMD_PP, 2u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
                IS25_NOR_CMD(IS25WX064_OCMD_RDSR, 2u, 4u, 8u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
                IS25_NOR_CMD(IS25WX064_OCMD_WRSR, 2u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
                IS25_NOR_CMD(IS25WX064_OCMD_WREN, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
                IS25_NOR_CMD(IS25WX064_OCMD_WRDI, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
                IS25_NOR_CMD(IS25WX064_OCMD_RDID, 2u, 4u, 8u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
                IS25_NOR_CMD(IS25WX064_OCMD_RDSFDP, 2u, 4u, 8u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
                IS25_NOR_CMD(IS25WX064_OCMD_SE_4K, 2u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
                IS25_NOR_CMD(IS25WX064_OCMD_BE_32K, 2u, 4u, 0u, 0u),
            [IS25WX064_CMD_BLOCK_ERASE_128K] =
                IS25_NOR_CMD(IS25WX064_OCMD_BE_128K, 2u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
                IS25_NOR_CMD(IS25WX064_OCMD_CE, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
                IS25_NOR_CMD(IS25WX064_OCMD_RESET_ENABLE, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
                IS25_NOR_CMD(IS25WX064_OCMD_RESET_MEMORY, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2] =
                IS25_NOR_CMD(IS25WX064_OCMD_RD_VCFGREG, 2u, 4u, 8u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2] =
                IS25_NOR_CMD(IS25WX064_OCMD_WR_VCFGREG, 2u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
                IS25_NOR_CMD(IS25WX064_OCMD_SUSPEND, 2u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
                IS25_NOR_CMD(IS25WX064_OCMD_RESUME, 2u, 0u, 0u, 0u),
        },
    },
};

static const am_devices_xspi_nor_jedec_cmd_profile_t g_sIS25WX064JEDECCmdProfiles[] =
{
    // all other modes uses generic handling
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .sCmdInfo  =
        {
            .ui16ReadCmd        = IS25WX064_OCMD_DTR_READ,
            .ui16PageProgramCmd = IS25WX064_OCMD_PP,
            .ui8ReadDummyCycles = 16u,
            .bUses4ByteAddr     = true,
            .bSFDPAddr4         = true,
            .bSFDPDummy20       = false,
            .bRDSRAddr4         = false,
            .ui8RDSRDummy       = 8u,
            .bRDIDAddr4         = false,
            .ui8RDIDDummy       = 8u,
            .eDeviceModeDomain  = AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_DTR,
            .eModeEnableReq     = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        },
    },
};

static const uint8_t g_ui8IS25WX064IOModeExtSPI[]   = {IS25WX064_CR_IO_EXT_SPI};
static const uint8_t g_ui8IS25WX064IOModeOctalDDR[] = {IS25WX064_CR_IO_OCTAL_DDR};

static const am_devices_xspi_nor_switch_op_t g_sIS25WX064ExtSPIOps[] =
{
    {
        .ui16Cmd      = IS25WX064_OCMD_WREN,
        .ui8CmdLen    = 2u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
    {
        .ui16Cmd      = IS25WX064_CMD_WR_VCFGREG,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = IS25WX064_CR_REG_ADDR_IO_MODE,
        .pui8Data     = g_ui8IS25WX064IOModeExtSPI,
        .ui32NumBytes = sizeof(g_ui8IS25WX064IOModeExtSPI),
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sIS25WX064OctalDDROps[] =
{
    {
        .ui16Cmd      = AM_DEVICES_XSPI_NOR_OP_WREN,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
    {
        .ui16Cmd      = IS25WX064_CMD_WR_VCFGREG,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = IS25WX064_CR_REG_ADDR_IO_MODE,
        .pui8Data     = g_ui8IS25WX064IOModeOctalDDR,
        .ui32NumBytes = sizeof(g_ui8IS25WX064IOModeOctalDDR),
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_mode_switch_t g_sIS25WX064ModeSwitches[] =
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
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE,
        .pEnterOps       = NULL,
        .ui32NumEnterOps = 0u,
        .pExitOps        = NULL,
        .ui32NumExitOps  = 0u,
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE,
        .pEnterOps       = NULL,
        .ui32NumEnterOps = 0u,
        .pExitOps        = NULL,
        .ui32NumExitOps  = 0u,
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        .pEnterOps       = g_sIS25WX064OctalDDROps,
        .ui32NumEnterOps = sizeof(g_sIS25WX064OctalDDROps) /
                           sizeof(g_sIS25WX064OctalDDROps[0]),
        .pExitOps        = g_sIS25WX064ExtSPIOps,
        .ui32NumExitOps  = sizeof(g_sIS25WX064ExtSPIOps) /
                           sizeof(g_sIS25WX064ExtSPIOps[0]),
    },
};

//*****************************************************************************
//
//! @brief Internal helper for xSPI NOR flow.
//
//*****************************************************************************
static uint32_t is25wx064_return_to_spi(am_hal_periph_handle_t *pFlash)
{
    if (am_devices_xspi_nor_write_enable(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          0x6666,
                                          2u,
                                          0u,
                                          0u,
                                          NULL,
                                          0u,
                                          0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          0x9999,
                                          2u,
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
is25wx064_post_reset(am_hal_periph_handle_t *pFlash,
                     const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg)
{
    uint8_t ui8FlagStatus = 0u;

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
                                           AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                           pProtocolCfg->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (is25wx064_return_to_spi(pFlash))
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

    for (uint32_t i = 0u; i < AM_DEVICES_XSPI_NOR_TIMEOUT_US; i++)
    {
        if (am_devices_xspi_nor_command_read(pFlash,
                                             IS25WX064_CMD_READ_FLAG_STATUS,
                                             1u,
                                             0u,
                                             0u,
                                             &ui8FlagStatus,
                                             1u,
                                             0u))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        if ((ui8FlagStatus & AM_DEVICES_XSPI_NOR_FSR_READY) != 0u)
        {
            return AM_DEVICES_XSPI_STATUS_SUCCESS;
        }

        (void)am_hal_periph_delay(pFlash, 1u);
    }

    return AM_DEVICES_XSPI_STATUS_ERROR;
}

static const am_devices_xspi_nor_quirks_t g_sIS25WX064Quirks =
{
    .soft_reset    = NULL,
    .post_reset    = is25wx064_post_reset,
    .post_init     = NULL,
    .read_jedec_id = NULL,
    .pre_read      = NULL,
    .post_read     = NULL,
    .pre_write     = NULL,
    .post_write    = NULL,
};

//
// Read-while-write profile (SPI 1S-1S-1S protocol).
//
// IS25WX064 suspend/resume use opcodes 75h/7Ah in extended SPI. Per the
// IS25LX/WX032-064 datasheet, Program/Erase Specifications:
//   Suspend latency: program 25 us max, subsector/erase 30 us max
//   Erase resume -> erase suspend: 150 us (typical)
//   Program resume -> program suspend: 5 us (typical)
//   tPP (page program)  = 1800 us max
//   tSE4K (4KB erase)   = 400 ms = 400,000 us max
//   tSE32K (32KB erase) = 1 s = 1,000,000 us max
//   128KB sector erase  ~ 2 s = 2,000,000 us (conservative, ~2x 64KB tSE)
// Chip erase is excluded (cannot be encoded in the 32-bit holdoff field).
// Note: the 128KB erase reuses the CMD_BLOCK_ERASE_64K enum slot by convention.
//
static const am_devices_xspi_nor_rww_op_t g_sIS25WX064RWWOps[] =
{
    { AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM,      1800u },
    { AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K,   400000u },
    { AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K,   1000000u },
    { AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K,   2000000u },
};

static const am_devices_xspi_nor_rww_cfg_t g_sIS25WX064RWWCfgs[] =
{
    {
        .eProtocol                 = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eSuspendCmd               = AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
        .eResumeCmd                = AM_DEVICES_XSPI_NOR_CMD_RESUME,
        .ui32SuspendTimeUs         = 30u,
        .ui32ResumeTimeUs          = 30u,
        .ui32SuspendToResumeTimeUs = 10u,
        .ui32ResumeToSuspendTimeUs = 150u,
        .ui32HoldoffGuardTimeUs    = 100u,
        .pOperations               = g_sIS25WX064RWWOps,
        .ui32NumOperations         = sizeof(g_sIS25WX064RWWOps) /
                                     sizeof(g_sIS25WX064RWWOps[0]),
    },
    {
        .eProtocol                 = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .eSuspendCmd               = AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
        .eResumeCmd                = AM_DEVICES_XSPI_NOR_CMD_RESUME,
        .ui32SuspendTimeUs         = 30u,
        .ui32ResumeTimeUs          = 30u,
        .ui32SuspendToResumeTimeUs = 10u,
        .ui32ResumeToSuspendTimeUs = 150u,
        .ui32HoldoffGuardTimeUs    = 100u,
        .pOperations               = g_sIS25WX064RWWOps,
        .ui32NumOperations         = sizeof(g_sIS25WX064RWWOps) /
                                     sizeof(g_sIS25WX064RWWOps[0]),
    },
};

const am_devices_xspi_nor_device_t g_sXSPINORIS25WX064 =
{
    .pName            = "IS25WX064",
    .ui8VendorId      = 0x9Du,
    .ui8DeviceId      = 0x5Bu,
    .ui32DeviceSizeKb = AM_DEVICES_XSPI_NOR_IS25WX064_FLASH_SIZE_KB,
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
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D),
    .ui8JedecIdReadLen    = 3u,
    .bSupport4ByteAddress = true,
    .bUse4ByteOpcodes     = false,
    .eEnter4ByteMethod    = AM_DEVICES_XSPI_NOR_ENTER_4BYTE_B7,
    .pEraseTypes          = g_sIS25WX064EraseTypes,
    .ui32NumEraseTypes    = sizeof(g_sIS25WX064EraseTypes) /
                            sizeof(g_sIS25WX064EraseTypes[0]),
    .pCmdSets                = g_sIS25WX064CmdSets,
    .ui32NumCmdSets          = sizeof(g_sIS25WX064CmdSets) / sizeof(g_sIS25WX064CmdSets[0]),
    .pJEDECCmdProfiles       = g_sIS25WX064JEDECCmdProfiles,
    .ui32NumJEDECCmdProfiles = sizeof(g_sIS25WX064JEDECCmdProfiles) /
                               sizeof(g_sIS25WX064JEDECCmdProfiles[0]),
    .pModeSwitches       = g_sIS25WX064ModeSwitches,
    .ui32NumModeSwitches = sizeof(g_sIS25WX064ModeSwitches) / sizeof(g_sIS25WX064ModeSwitches[0]),
    .pRWWCfgs            = g_sIS25WX064RWWCfgs,
    .ui32NumRWWCfgs      = sizeof(g_sIS25WX064RWWCfgs) / sizeof(g_sIS25WX064RWWCfgs[0]),
    .pQuirks             = &g_sIS25WX064Quirks,
};

//*****************************************************************************
//
//! @brief Internal helper for xSPI NOR flow.
//
//*****************************************************************************
static uint32_t
is25wx064_config(am_device_xspi_periph_handle_t *pFlash,
                 const am_devices_xspi_norflash_cfg_t *pConfig)
{
    return am_devices_xspi_nor_config(pFlash, pConfig, &g_sXSPINORIS25WX064);
}

//*****************************************************************************
//
//! @brief Internal helper for xSPI NOR flow.
//
//*****************************************************************************
static uint32_t
is25wx064_power(am_device_xspi_periph_handle_t *pFlash,
                am_devices_xspi_power_e ePowerMode,
                const am_devices_xspi_nor_power_cfg_t *pPowerCfg)
{
    return am_devices_xspi_nor_power_control(pFlash, ePowerMode, pPowerCfg);
}

const am_device_xspi_func_t apiIS25WX064 =
{
    .device_init   = am_devices_xspi_nor_init,
    .device_deinit = am_devices_xspi_nor_deinit,
    .device_config = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                   const am_devices_xspi_config_t *,
                                   ...)) is25wx064_config,
    .device_get_info      = am_devices_xspi_nor_get_info,
    .device_write         = am_devices_xspi_nor_write,
    .device_read          = am_devices_xspi_nor_read,
    .device_erase         = am_devices_xspi_nor_erase,
    .device_set_xfer_mode = am_devices_xspi_nor_set_xfer_mode,
    .device_power_ctrl    = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) is25wx064_power,
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
