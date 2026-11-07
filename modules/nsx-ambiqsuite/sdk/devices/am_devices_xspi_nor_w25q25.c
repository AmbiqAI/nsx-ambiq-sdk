//*****************************************************************************
//
//! @file am_devices_xspi_nor_w25q25.c
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

#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor_w25q25.h"

#if AM_DEVICES_XSPI_NOR_ENABLE_W25Q25

#define W25Q25_OP_FAST_READ_DUAL_OUTPUT       0x3Bu
#define W25Q25_OP_FAST_READ_DUAL_OUTPUT_4B    0x3Cu
#define W25Q25_OP_DTR_FAST_READ_DUAL_IO       0xBDu
#define W25Q25_OP_DTR_FAST_READ_QUAD_IO       0xEDu
#define W25Q25_OP_DTR_FAST_READ_QUAD_IO_4B    0xEEu
#define W25Q25_OP_ENTER_QPI                   0x38u
#define W25Q25_OP_EXIT_QPI                    0xFFu
#define W25Q25_MODE_BITS_RESET_CMD            0xFFu

#define W25Q25_CMD(_cmd, _cmd4, _alen, _rxd, _mlen, _mode)   \
        {                                                    \
            .ui16Cmd       = (_cmd),                         \
            .ui16Cmd4Byte  = (_cmd4),                        \
            .ui8CmdLen     = 1u,                             \
            .ui8AddrLen    = (_alen),                        \
            .ui8RxDummy    = (_rxd),                         \
            .ui8TxDummy    = 0u,                             \
            .ui8ModeBitLen = (_mlen),                        \
            .ui32ModeBits  = (_mode),                        \
        }

#define W25Q25_JEDEC_PROFILE(_proto, _read, _pp, _dummy, _domain, _enable)                 \
        {                                                                                  \
            .eProtocol = (_proto),                                                         \
            .sCmdInfo  =                                                                   \
            {                                                                              \
                .ui16ReadCmd        = (_read),                                             \
                .ui16PageProgramCmd = (_pp),                                               \
                .ui8ReadDummyCycles = (_dummy),                                            \
                .bUses4ByteAddr     = true,                                                \
                .bSFDPAddr4         = false,                                               \
                .bSFDPDummy20       = false,                                               \
                .bRDSRAddr4         = false,                                               \
                .ui8RDSRDummy       = 0u,                                                  \
                .bRDIDAddr4         = false,                                               \
                .ui8RDIDDummy       = 0u,                                                  \
                .eDeviceModeDomain  = (_domain),                                           \
                .eModeEnableReq     = (_enable),                                           \
            },                                                                             \
        }

static const am_devices_xspi_nor_erase_type_t g_sW25Q25EraseTypes[] =
{
    { 4u * 1024u,  AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K },
    { 32u * 1024u, AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K },
    { 64u * 1024u, AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K },
};

static const am_devices_xspi_nor_cmd_fallback_t g_sW25Q25CmdFallbacks[] =
{
    {
        .eSourceProtocol   = AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
        .eFallbackProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
        .ui32CmdMask       =
            AM_DEVICES_XSPI_NOR_CMD_BIT(AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM),
    },
    {
        .eSourceProtocol   = AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D,
        .eFallbackProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
        .ui32CmdMask       =
            ~AM_DEVICES_XSPI_NOR_CMD_BIT(AM_DEVICES_XSPI_NOR_CMD_READ),
    },
    {
        .eSourceProtocol   = AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D,
        .eFallbackProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .ui32CmdMask       =
            ~AM_DEVICES_XSPI_NOR_CMD_BIT(AM_DEVICES_XSPI_NOR_CMD_READ),
    },
    // The following uses uses generic resolver
    // AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D -> AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S
    // AM_DEVICES_XSPI_NOR_PROTO_1S_2S/D_2S/D -> AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S
    // AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S -> AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S
};

static const am_devices_xspi_nor_cmd_set_t g_sW25Q25CmdSets[] =
{
    // generic except erase 32K
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eIOMode   = AM_HAL_XSPIMC_IO_SERIAL,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_READ_FAST,
                           AM_DEVICES_XSPI_NOR_OP_READ_FAST_4B,
                           3, 8, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_PP,
                           AM_DEVICES_XSPI_NOR_OP_PP_4B,
                           3, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR,
                           AM_DEVICES_XSPI_NOR_OP_RDSR,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR,
                           AM_DEVICES_XSPI_NOR_OP_WRSR,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR2,
                           AM_DEVICES_XSPI_NOR_OP_RDSR2,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR2,
                           AM_DEVICES_XSPI_NOR_OP_WRSR2,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS3] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR3,
                           AM_DEVICES_XSPI_NOR_OP_RDSR3,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS3] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR3,
                           AM_DEVICES_XSPI_NOR_OP_WRSR3,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WREN,
                           AM_DEVICES_XSPI_NOR_OP_WREN,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRDI,
                           AM_DEVICES_XSPI_NOR_OP_WRDI,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDID,
                           AM_DEVICES_XSPI_NOR_OP_RDID,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSFDP,
                           AM_DEVICES_XSPI_NOR_OP_RDSFDP,
                           3, 8, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_SE,
                           AM_DEVICES_XSPI_NOR_OP_SE_4B,
                           3, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_BE_32K,
                           AM_DEVICES_XSPI_NOR_OP_BE_32K,
                           3, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_BE,
                           AM_DEVICES_XSPI_NOR_OP_BE_4B,
                           3, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_CE,
                           AM_DEVICES_XSPI_NOR_OP_CE,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE,
                           AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE,
                           AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_EN,
                           AM_DEVICES_XSPI_NOR_OP_RESET_EN,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_MEM,
                           AM_DEVICES_XSPI_NOR_OP_RESET_MEM,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDCR,
                           AM_DEVICES_XSPI_NOR_OP_RDCR,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WR_CFGREG2,
                           AM_DEVICES_XSPI_NOR_OP_WR_CFGREG2,
                           4, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RD_CFGREG2,
                           AM_DEVICES_XSPI_NOR_OP_RD_CFGREG2,
                           4, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_SUSPEND,
                           AM_DEVICES_XSPI_NOR_OP_SUSPEND,
                           0, 0, 0, 0),
            [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESUME,
                           AM_DEVICES_XSPI_NOR_OP_RESUME,
                           0, 0, 0, 0),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_1_2,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(W25Q25_OP_FAST_READ_DUAL_OUTPUT,
                           W25Q25_OP_FAST_READ_DUAL_OUTPUT_4B,
                           3u, 8u, 0u, 0u),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_2_2,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ,
                           AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ_4B,
                           3u, 0u, 1u, W25Q25_MODE_BITS_RESET_CMD),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_1_4,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_QUAD_READ,
                           AM_DEVICES_XSPI_NOR_OP_QUAD_READ_4B,
                           3u, 4u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_PP_1_1_4,
                           AM_DEVICES_XSPI_NOR_OP_PP_1_1_4_4B,
                           3u, 0u, 0u, 0u),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_4_4,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ,
                           AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ_4B,
                           3u, 4u,
                           1u, W25Q25_MODE_BITS_RESET_CMD),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .eIOMode   = AM_HAL_XSPIMC_IO_QUAD,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ,
                           AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ_4B,
                           3u, 4u,
                           1u, W25Q25_MODE_BITS_RESET_CMD),
            [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_PP,
                           AM_DEVICES_XSPI_NOR_OP_PP_4B,
                           3u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR,
                           AM_DEVICES_XSPI_NOR_OP_RDSR,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR,
                           AM_DEVICES_XSPI_NOR_OP_WRSR,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR2,
                           AM_DEVICES_XSPI_NOR_OP_RDSR2,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS2] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR2,
                           AM_DEVICES_XSPI_NOR_OP_WRSR2,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS3] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDSR3,
                           AM_DEVICES_XSPI_NOR_OP_RDSR3,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS3] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRSR3,
                           AM_DEVICES_XSPI_NOR_OP_WRSR3,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WREN,
                           AM_DEVICES_XSPI_NOR_OP_WREN,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_WRDI,
                           AM_DEVICES_XSPI_NOR_OP_WRDI,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RDID,
                           AM_DEVICES_XSPI_NOR_OP_RDID,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_SE,
                           AM_DEVICES_XSPI_NOR_OP_SE_4B,
                           3u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_BE_32K,
                           AM_DEVICES_XSPI_NOR_OP_BE_32K,
                           3u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_BE,
                           AM_DEVICES_XSPI_NOR_OP_BE_4B,
                           3u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_CE,
                           AM_DEVICES_XSPI_NOR_OP_CE,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_EN,
                           AM_DEVICES_XSPI_NOR_OP_RESET_EN,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESET_MEM,
                           AM_DEVICES_XSPI_NOR_OP_RESET_MEM,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_SUSPEND,
                           AM_DEVICES_XSPI_NOR_OP_SUSPEND,
                           0u, 0u, 0u, 0u),
            [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
                W25Q25_CMD(AM_DEVICES_XSPI_NOR_OP_RESUME,
                           AM_DEVICES_XSPI_NOR_OP_RESUME,
                           0u, 0u, 0u, 0u),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_2_2,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_S_D_D,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(W25Q25_OP_DTR_FAST_READ_DUAL_IO,
                           W25Q25_OP_DTR_FAST_READ_DUAL_IO, //non-generic
                           3u, 4u, 1u, W25Q25_MODE_BITS_RESET_CMD),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D,
        .eIOMode   = AM_HAL_XSPIMC_IO_1_4_4,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_S_D_D,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(W25Q25_OP_DTR_FAST_READ_QUAD_IO,
                           W25Q25_OP_DTR_FAST_READ_QUAD_IO_4B,
                           3u, 7u, 1u, W25Q25_MODE_BITS_RESET_CMD),
        },
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D,
        .eIOMode   = AM_HAL_XSPIMC_IO_QUAD,
        .eDataRate = AM_HAL_XSPIMC_DATA_RATE_S_D_D,
        .asCmdInfo =
        {
            [AM_DEVICES_XSPI_NOR_CMD_READ] =
                W25Q25_CMD(W25Q25_OP_DTR_FAST_READ_QUAD_IO,
                           W25Q25_OP_DTR_FAST_READ_QUAD_IO_4B,
                           3u, 7u, 1u, W25Q25_MODE_BITS_RESET_CMD),
        },
    },
};

static const am_devices_xspi_nor_jedec_cmd_profile_t g_sW25Q25JEDECCmdProfiles[] =
{
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
                         AM_DEVICES_XSPI_NOR_OP_READ_FAST,
                         AM_DEVICES_XSPI_NOR_OP_PP,
                         8u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_SPI,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S,
                         W25Q25_OP_FAST_READ_DUAL_OUTPUT,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         8u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S,
                         AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         0u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
                         AM_DEVICES_XSPI_NOR_OP_QUAD_READ,
                         AM_DEVICES_XSPI_NOR_OP_PP_1_1_4,
                         8u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
                         AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         4u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
                         AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ,
                         AM_DEVICES_XSPI_NOR_OP_PP,
                         4u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_STR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D,
                         W25Q25_OP_DTR_FAST_READ_DUAL_IO,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         4u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_DTR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D,
                         W25Q25_OP_DTR_FAST_READ_QUAD_IO,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         7u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_DTR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6),
    W25Q25_JEDEC_PROFILE(AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D,
                         W25Q25_OP_DTR_FAST_READ_QUAD_IO,
                         AM_DEVICES_XSPI_NOR_OP_NONE,
                         7u,
                         AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_DTR,
                         AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6),
};

static const am_devices_xspi_nor_switch_op_t g_sW25Q25QPIEnterOps[] =
{
    {
        .ui16Cmd      = W25Q25_OP_ENTER_QPI,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sW25Q25QPIExitOps[] =
{
    {
        .ui16Cmd      = W25Q25_OP_EXIT_QPI,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 0u,
        .ui32Addr     = 0u,
        .pui8Data     = NULL,
        .ui32NumBytes = 0u,
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_mode_switch_t g_sW25Q25ModeSwitches[] =
{
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6,
        .bDisableOnExit  = false,
        .pEnterOps       = g_sW25Q25QPIEnterOps,
        .ui32NumEnterOps = sizeof(g_sW25Q25QPIEnterOps) /
                           sizeof(g_sW25Q25QPIEnterOps[0]),
        .pExitOps        = g_sW25Q25QPIExitOps,
        .ui32NumExitOps  = sizeof(g_sW25Q25QPIExitOps) /
                           sizeof(g_sW25Q25QPIExitOps[0]),
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6,
        .bDisableOnExit  = false,
        .pEnterOps       = g_sW25Q25QPIEnterOps,
        .ui32NumEnterOps = sizeof(g_sW25Q25QPIEnterOps) /
                           sizeof(g_sW25Q25QPIEnterOps[0]),
        .pExitOps        = g_sW25Q25QPIExitOps,
        .ui32NumExitOps  = sizeof(g_sW25Q25QPIExitOps) /
                           sizeof(g_sW25Q25QPIExitOps[0]),
    },
};

//*****************************************************************************
//
//! @brief Exit QPI with the FFh instruction sent as a 4-0-0 transaction.
//
//*****************************************************************************
static uint32_t
w25q25_soft_reset(am_hal_periph_handle_t *pFlash,
                  const am_devices_xspi_nor_protocol_cfg_t *pProtocolCfg)
{
    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
                                           AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                           pProtocolCfg->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_command_write(pFlash,
                                          W25Q25_OP_EXIT_QPI,
                                          1u,
                                          0u,
                                          0u,
                                          NULL,
                                          0u,
                                          0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return am_devices_xspi_nor_apply_protocol(pFlash,
                                              AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
                                              AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ,
                                              pProtocolCfg->bDQS);
}

static const am_devices_xspi_nor_quirks_t g_sW25Q25Quirks =
{
    .soft_reset    = w25q25_soft_reset,
    .post_reset    = NULL,
    .post_init     = NULL,
    .read_jedec_id = NULL,
    .pre_read      = NULL,
    .post_read     = NULL,
    .pre_write     = NULL,
    .post_write    = NULL,
};

//
// Read-while-write profiles (SPI 1S-1S-1S and QPI 4S-4S-4S).
//
// Winbond W25Q25PW_DTR suspend/resume use opcodes 75h/7Ah. Per the Winbond
// datasheet, tSUS is the max latency to suspend (BUSY clears within tSUS) and
// is also the minimum spacing required between a Resume (7Ah) and the next
// Suspend (75h). Worst-case busy times (typical Winbond W25Q-series values):
//   page program  = 3 ms   = 3,000 us
//   sector erase  = 400 ms  = 400,000 us
//   block erase 32K = 1600 ms = 1,600,000 us
//   block erase 64K = 2000 ms = 2,000,000 us
// Chip erase is excluded (cannot be encoded in the 32-bit holdoff field).
//
static const am_devices_xspi_nor_rww_op_t g_sW25Q25RWWOps[] =
{
    { AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM,      3000u },
    { AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K,   400000u },
    { AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K,   1600000u },
    { AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K,   2000000u },
};

static const am_devices_xspi_nor_rww_cfg_t g_sW25Q25RWWCfgs[] =
{
    {
        .eProtocol                 = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eSuspendCmd               = AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
        .eResumeCmd                = AM_DEVICES_XSPI_NOR_CMD_RESUME,
        .ui32SuspendTimeUs         = 30u,
        .ui32ResumeTimeUs          = 30u,
        .ui32SuspendToResumeTimeUs = 10u,
        .ui32ResumeToSuspendTimeUs = 30u,
        .ui32HoldoffGuardTimeUs    = 100u,
        .pOperations               = g_sW25Q25RWWOps,
        .ui32NumOperations         = sizeof(g_sW25Q25RWWOps) /
                                     sizeof(g_sW25Q25RWWOps[0]),
    },
    {
        .eProtocol                 = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .eSuspendCmd               = AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
        .eResumeCmd                = AM_DEVICES_XSPI_NOR_CMD_RESUME,
        .ui32SuspendTimeUs         = 30u,
        .ui32ResumeTimeUs          = 30u,
        .ui32SuspendToResumeTimeUs = 10u,
        .ui32ResumeToSuspendTimeUs = 30u,
        .ui32HoldoffGuardTimeUs    = 100u,
        .pOperations               = g_sW25Q25RWWOps,
        .ui32NumOperations         = sizeof(g_sW25Q25RWWOps) /
                                     sizeof(g_sW25Q25RWWOps[0]),
    },
};

const am_devices_xspi_nor_device_t g_sXSPINORW25Q25 =
{
    .pName            = "W25Q25PW_DTR",
    .ui8VendorId      = 0xEFu,
    .ui8DeviceId      = 0x80u,
    .ui32DeviceSizeKb = AM_DEVICES_XSPI_NOR_W25Q25_FLASH_SIZE_KB,
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
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D),
    .ui8JedecIdReadLen       = 3u,
    .bSupport4ByteAddress    = true,
    .bUse4ByteOpcodes        = true,
    .eEnter4ByteMethod       = AM_DEVICES_XSPI_NOR_ENTER_4BYTE_B7,
    .pEraseTypes             = g_sW25Q25EraseTypes,
    .ui32NumEraseTypes       = sizeof(g_sW25Q25EraseTypes) /
                               sizeof(g_sW25Q25EraseTypes[0]),
    .pCmdSets                = g_sW25Q25CmdSets,
    .ui32NumCmdSets          = sizeof(g_sW25Q25CmdSets) /
                               sizeof(g_sW25Q25CmdSets[0]),
    .pCmdFallbacks           = g_sW25Q25CmdFallbacks,
    .ui32NumCmdFallbacks     = sizeof(g_sW25Q25CmdFallbacks) /
                               sizeof(g_sW25Q25CmdFallbacks[0]),
    .pJEDECCmdProfiles       = g_sW25Q25JEDECCmdProfiles,
    .ui32NumJEDECCmdProfiles = sizeof(g_sW25Q25JEDECCmdProfiles) /
                               sizeof(g_sW25Q25JEDECCmdProfiles[0]),
    .pModeSwitches           = g_sW25Q25ModeSwitches,
    .ui32NumModeSwitches     = sizeof(g_sW25Q25ModeSwitches) /
                               sizeof(g_sW25Q25ModeSwitches[0]),
    .pRWWCfgs                = g_sW25Q25RWWCfgs,
    .ui32NumRWWCfgs          = sizeof(g_sW25Q25RWWCfgs) /
                               sizeof(g_sW25Q25RWWCfgs[0]),
    .pQuirks                 = &g_sW25Q25Quirks,
};

//*****************************************************************************
//
//! @brief Configure the W25Q25PW_DTR through the generic NOR core.
//
//*****************************************************************************
static uint32_t
w25q25_config(am_device_xspi_periph_handle_t *pFlash,
              const am_devices_xspi_norflash_cfg_t *pConfig)
{
    return am_devices_xspi_nor_config(pFlash, pConfig, &g_sXSPINORW25Q25);
}

//*****************************************************************************
//
//! @brief Control W25Q25PW_DTR power through the generic NOR core.
//
//*****************************************************************************
static uint32_t
w25q25_power(am_device_xspi_periph_handle_t *pFlash,
             am_devices_xspi_power_e ePowerMode,
             const am_devices_xspi_nor_power_cfg_t *pPowerCfg)
{
    return am_devices_xspi_nor_power_control(pFlash, ePowerMode, pPowerCfg);
}

const am_device_xspi_func_t apiW25Q25 =
{
    .device_init   = am_devices_xspi_nor_init,
    .device_deinit = am_devices_xspi_nor_deinit,
    .device_config = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                   const am_devices_xspi_config_t *,
                                   ...)) w25q25_config,
    .device_get_info      = am_devices_xspi_nor_get_info,
    .device_write         = am_devices_xspi_nor_write,
    .device_read          = am_devices_xspi_nor_read,
    .device_erase         = am_devices_xspi_nor_erase,
    .device_set_xfer_mode = am_devices_xspi_nor_set_xfer_mode,
    .device_power_ctrl    = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) w25q25_power,
    .device_mem_map_config = am_devices_xspi_nor_mem_map_config,
    .device_timing_apply   = am_devices_xspi_nor_timing_apply,
};

#undef W25Q25_JEDEC_PROFILE
#undef W25Q25_CMD

#endif

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
