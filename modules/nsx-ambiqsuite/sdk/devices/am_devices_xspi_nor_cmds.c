//*****************************************************************************
//
//! @file am_devices_xspi_nor_cmds.c
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

#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor_cmds.h"

#define NOR_CMD(_cmd, _cmd4, _clen, _alen, _rxd, _txd)             \
        {                                                          \
            .ui16Cmd       = (_cmd),                               \
            .ui16Cmd4Byte  = (_cmd4),                              \
            .ui8CmdLen     = (_clen),                              \
            .ui8AddrLen    = (_alen),                              \
            .ui8RxDummy    = (_rxd),                               \
            .ui8TxDummy    = (_txd),                               \
            .ui8ModeBitLen = 0,                                    \
            .ui32ModeBits  = 0,                                    \
        }

#define NOR_JEDEC_CMD_INFO(_read, _pp, _dummy, _use4b,                       \
                           _sfdp4, _sfdp20, _rdsr4, _rdsrd, _rdid4, _rdidd)  \
        {                                                                    \
            .ui16ReadCmd        = (_read),                                   \
            .ui16PageProgramCmd = (_pp),                                     \
            .ui8ReadDummyCycles = (_dummy),                                  \
            .bUses4ByteAddr     = (_use4b),                                  \
            .bSFDPAddr4         = (_sfdp4),                                  \
            .bSFDPDummy20       = (_sfdp20),                                 \
            .bRDSRAddr4         = (_rdsr4),                                  \
            .ui8RDSRDummy       = (_rdsrd),                                  \
            .bRDIDAddr4         = (_rdid4),                                  \
            .ui8RDIDDummy       = (_rdidd),                                  \
            .eDeviceModeDomain  = AM_DEVICES_XSPI_NOR_MODE_DOMAIN_AUTO,      \
            .eModeEnableReq     = AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE,      \
        }

#define OP_NONE                  AM_DEVICES_XSPI_NOR_OP_NONE

#define OP_WRSR                  AM_DEVICES_XSPI_NOR_OP_WRSR
#define OP_RDSR                  AM_DEVICES_XSPI_NOR_OP_RDSR
#define OP_WRSR2                 AM_DEVICES_XSPI_NOR_OP_WRSR2
#define OP_RDSR2                 AM_DEVICES_XSPI_NOR_OP_RDSR2
#define OP_WRSR3                 AM_DEVICES_XSPI_NOR_OP_WRSR3
#define OP_RDSR3                 AM_DEVICES_XSPI_NOR_OP_RDSR3
#define OP_RDCR                  AM_DEVICES_XSPI_NOR_OP_RDCR

#define OP_READ_FAST             AM_DEVICES_XSPI_NOR_OP_READ_FAST
#define OP_READ_FAST_4B          AM_DEVICES_XSPI_NOR_OP_READ_FAST_4B
#define OP_DUAL_IO_READ          AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ
#define OP_DUAL_IO_READ_4B       AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ_4B
#define OP_QUAD_READ             AM_DEVICES_XSPI_NOR_OP_QUAD_READ
#define OP_QUAD_READ_4B          AM_DEVICES_XSPI_NOR_OP_QUAD_READ_4B
#define OP_QUAD_IO_READ          AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ
#define OP_QUAD_IO_READ_4B       AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ_4B
#define OP_OCTA_1_1_8_READ       AM_DEVICES_XSPI_NOR_OP_1_1_8_READ
#define OP_OCTA_1_1_8_READ_4B    AM_DEVICES_XSPI_NOR_OP_1_1_8_READ_4B
#define OP_OCTA_1_8_8_READ       AM_DEVICES_XSPI_NOR_OP_1_8_8_READ
#define OP_OCTA_1_8_8_READ_4B    AM_DEVICES_XSPI_NOR_OP_1_8_8_READ_4B
#define OP_OCTA_1_8_8_DTR_4B     AM_DEVICES_XSPI_NOR_OP_1_8_8_READ_DTR_4B

#define OP_WREN                  AM_DEVICES_XSPI_NOR_OP_WREN
#define OP_WRDI                  AM_DEVICES_XSPI_NOR_OP_WRDI
#define OP_PP                    AM_DEVICES_XSPI_NOR_OP_PP
#define OP_PP_4B                 AM_DEVICES_XSPI_NOR_OP_PP_4B
#define OP_PP_1_1_2              AM_DEVICES_XSPI_NOR_OP_PP_1_1_2
#define OP_PP_1_1_4              AM_DEVICES_XSPI_NOR_OP_PP_1_1_4
#define OP_PP_1_1_4_4B           AM_DEVICES_XSPI_NOR_OP_PP_1_1_4_4B
#define OP_PP_1_4_4              AM_DEVICES_XSPI_NOR_OP_PP_1_4_4
#define OP_PP_1_4_4_4B           AM_DEVICES_XSPI_NOR_OP_PP_1_4_4_4B
#define OP_PP_1_1_8              AM_DEVICES_XSPI_NOR_OP_PP_1_1_8
#define OP_PP_1_1_8_4B           AM_DEVICES_XSPI_NOR_OP_PP_1_1_8_4B
#define OP_PP_1_8_8              AM_DEVICES_XSPI_NOR_OP_PP_1_8_8
#define OP_PP_1_8_8_4B           AM_DEVICES_XSPI_NOR_OP_PP_1_8_8_4B

#define OP_RDID                  AM_DEVICES_XSPI_NOR_OP_RDID
#define OP_RDSFDP                AM_DEVICES_XSPI_NOR_OP_RDSFDP
#define OP_SE                    AM_DEVICES_XSPI_NOR_OP_SE
#define OP_SE_4B                 AM_DEVICES_XSPI_NOR_OP_SE_4B
#define OP_BE_32K                AM_DEVICES_XSPI_NOR_OP_BE_32K
#define OP_BE_32K_4B             AM_DEVICES_XSPI_NOR_OP_BE_32K_4B
#define OP_BE                    AM_DEVICES_XSPI_NOR_OP_BE
#define OP_BE_4B                 AM_DEVICES_XSPI_NOR_OP_BE_4B
#define OP_CE                    AM_DEVICES_XSPI_NOR_OP_CE
#define OP_CE_ALT                AM_DEVICES_XSPI_NOR_OP_CE_ALT
#define OP_ENTER_4BYTE           AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE
#define OP_EXIT_4BYTE            AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE
#define OP_RESET_EN              AM_DEVICES_XSPI_NOR_OP_RESET_EN
#define OP_RESET_MEM             AM_DEVICES_XSPI_NOR_OP_RESET_MEM
#define OP_WR_CFGREG2            AM_DEVICES_XSPI_NOR_OP_WR_CFGREG2
#define OP_RD_CFGREG2            AM_DEVICES_XSPI_NOR_OP_RD_CFGREG2

#define OCMD_RD                  AM_DEVICES_XSPI_NOR_OCMD_RD
#define OCMD_DTR_RD              AM_DEVICES_XSPI_NOR_OCMD_DTR_RD
#define OCMD_PP                  AM_DEVICES_XSPI_NOR_OCMD_PP
#define OCMD_RDSR                AM_DEVICES_XSPI_NOR_OCMD_RDSR
#define OCMD_WRSR                AM_DEVICES_XSPI_NOR_OCMD_WRSR
#define OCMD_WREN                AM_DEVICES_XSPI_NOR_OCMD_WREN
#define OCMD_WRDI                AM_DEVICES_XSPI_NOR_OCMD_WRDI
#define OCMD_RDID                AM_DEVICES_XSPI_NOR_OCMD_RDID
#define OCMD_RDSFDP              AM_DEVICES_XSPI_NOR_OCMD_RDSFDP
#define OCMD_SE                  AM_DEVICES_XSPI_NOR_OCMD_SE
#define OCMD_BE                  AM_DEVICES_XSPI_NOR_OCMD_BE
#define OCMD_CE                  AM_DEVICES_XSPI_NOR_OCMD_CE
#define OCMD_CE_ALT              AM_DEVICES_XSPI_NOR_OCMD_CE_ALT
#define OCMD_RESET_EN            AM_DEVICES_XSPI_NOR_OCMD_RESET_EN
#define OCMD_RESET_MEM           AM_DEVICES_XSPI_NOR_OCMD_RESET_MEM
#define OCMD_WR_CFGREG2          AM_DEVICES_XSPI_NOR_OCMD_WR_CFGREG2
#define OCMD_RD_CFGREG2          AM_DEVICES_XSPI_NOR_OCMD_RD_CFGREG2

static const am_devices_xspi_nor_cmd_set_t g_sProto1S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
    .eIOMode   = AM_HAL_XSPIMC_IO_SERIAL,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_READ_FAST, OP_READ_FAST_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP, OP_PP_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
            NOR_CMD(OP_RDSR, OP_RDSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
            NOR_CMD(OP_WRSR, OP_WRSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS2] =
            NOR_CMD(OP_RDSR2, OP_RDSR2, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS2] =
            NOR_CMD(OP_WRSR2, OP_WRSR2, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS3] =
            NOR_CMD(OP_RDSR3, OP_RDSR3, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS3] =
            NOR_CMD(OP_WRSR3, OP_WRSR3, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
            NOR_CMD(OP_WREN, OP_WREN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
            NOR_CMD(OP_WRDI, OP_WRDI, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
            NOR_CMD(OP_RDID, OP_RDID, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
            NOR_CMD(OP_RDSFDP, OP_RDSFDP, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
            NOR_CMD(OP_SE, OP_SE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
            NOR_CMD(OP_BE_32K, OP_BE_32K_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
            NOR_CMD(OP_BE, OP_BE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
            NOR_CMD(OP_CE, OP_CE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE] =
            NOR_CMD(OP_ENTER_4BYTE, OP_ENTER_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE] =
            NOR_CMD(OP_EXIT_4BYTE, OP_EXIT_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
            NOR_CMD(OP_RESET_EN, OP_RESET_EN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
            NOR_CMD(OP_RESET_MEM, OP_RESET_MEM, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG] =
            NOR_CMD(OP_RDCR, OP_RDCR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2] =
            NOR_CMD(OP_WR_CFGREG2, OP_WR_CFGREG2, 1, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2] =
            NOR_CMD(OP_RD_CFGREG2, OP_RD_CFGREG2, 1, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_SUSPEND, AM_DEVICES_XSPI_NOR_OP_SUSPEND, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RESUME, AM_DEVICES_XSPI_NOR_OP_RESUME, 1, 0, 0, 0),
    },
};

static const am_devices_xspi_nor_cmd_set_t g_sProto2S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S,
    .eIOMode   = AM_HAL_XSPIMC_IO_DUAL,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_READ_FAST, OP_READ_FAST_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP, OP_PP_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
            NOR_CMD(OP_RDSR, OP_RDSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
            NOR_CMD(OP_WRSR, OP_WRSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
            NOR_CMD(OP_WREN, OP_WREN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
            NOR_CMD(OP_WRDI, OP_WRDI, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
            NOR_CMD(OP_RDID, OP_RDID, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
            NOR_CMD(OP_RDSFDP, OP_RDSFDP, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
            NOR_CMD(OP_SE, OP_SE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
            NOR_CMD(OP_BE_32K, OP_BE_32K_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
            NOR_CMD(OP_BE, OP_BE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
            NOR_CMD(OP_CE, OP_CE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE] =
            NOR_CMD(OP_ENTER_4BYTE, OP_ENTER_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE] =
            NOR_CMD(OP_EXIT_4BYTE, OP_EXIT_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
            NOR_CMD(OP_RESET_EN, OP_RESET_EN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
            NOR_CMD(OP_RESET_MEM, OP_RESET_MEM, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_SUSPEND, AM_DEVICES_XSPI_NOR_OP_SUSPEND, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RESUME, AM_DEVICES_XSPI_NOR_OP_RESUME, 1, 0, 0, 0),
    },
};

static const am_devices_xspi_nor_cmd_set_t g_sProto4S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
    .eIOMode   = AM_HAL_XSPIMC_IO_QUAD,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_READ_FAST, OP_READ_FAST_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP, OP_PP_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
            NOR_CMD(OP_RDSR, OP_RDSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
            NOR_CMD(OP_WRSR, OP_WRSR, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
            NOR_CMD(OP_WREN, OP_WREN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
            NOR_CMD(OP_WRDI, OP_WRDI, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
            NOR_CMD(OP_RDID, OP_RDID, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
            NOR_CMD(OP_RDSFDP, OP_RDSFDP, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
            NOR_CMD(OP_SE, OP_SE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
            NOR_CMD(OP_BE_32K, OP_BE_32K_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
            NOR_CMD(OP_BE, OP_BE_4B, 1, 3, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
            NOR_CMD(OP_CE, OP_CE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE] =
            NOR_CMD(OP_ENTER_4BYTE, OP_ENTER_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE] =
            NOR_CMD(OP_EXIT_4BYTE, OP_EXIT_4BYTE, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
            NOR_CMD(OP_RESET_EN, OP_RESET_EN, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
            NOR_CMD(OP_RESET_MEM, OP_RESET_MEM, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SUSPEND] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_SUSPEND, AM_DEVICES_XSPI_NOR_OP_SUSPEND, 1, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESUME] =
            NOR_CMD(AM_DEVICES_XSPI_NOR_OP_RESUME, AM_DEVICES_XSPI_NOR_OP_RESUME, 1, 0, 0, 0),
    },
};

static const am_devices_xspi_nor_cmd_set_t g_sProto1S1S4S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
    .eIOMode   = AM_HAL_XSPIMC_IO_1_1_4,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_QUAD_READ, OP_QUAD_READ_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP_1_1_4, OP_PP_1_1_4_4B, 1, 3, 0, 0),
    },
};

static const am_devices_xspi_nor_cmd_set_t g_sProto1S4S4S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
    .eIOMode   = AM_HAL_XSPIMC_IO_1_4_4,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_QUAD_IO_READ, OP_QUAD_IO_READ_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP_1_4_4, OP_PP_1_4_4_4B, 1, 3, 0, 0),
    },
};
//mirrors is25wx064
static const am_devices_xspi_nor_cmd_set_t g_sProto1S1S8S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S,
    .eIOMode   = AM_HAL_XSPIMC_IO_1_1_8,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_OCTA_1_1_8_READ, OP_OCTA_1_1_8_READ_4B, 1, 3, 8, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP_1_1_8, OP_PP_1_1_8_4B, 1, 3, 0, 0),
    },
};
//mirrors is25wx064
static const am_devices_xspi_nor_cmd_set_t g_sProto1S8S8S =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S,
    .eIOMode   = AM_HAL_XSPIMC_IO_1_8_8,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_SINGLE,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OP_OCTA_1_8_8_READ, OP_OCTA_1_8_8_READ_4B, 1, 3, 16, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OP_PP_1_8_8, OP_PP_1_8_8_4B, 1, 3, 0, 0),
    },
};

static const am_devices_xspi_nor_cmd_set_t g_sProto8D =
{
    .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
    .eIOMode   = AM_HAL_XSPIMC_IO_OCTAL,
    .eDataRate = AM_HAL_XSPIMC_DATA_RATE_DUAL,
    .asCmdInfo =
    {
        [AM_DEVICES_XSPI_NOR_CMD_READ] =
            NOR_CMD(OCMD_DTR_RD, OCMD_DTR_RD, 2, 4, 6, 0),
        [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM] =
            NOR_CMD(OCMD_PP, OCMD_PP, 2, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS] =
            NOR_CMD(OCMD_RDSR, OCMD_RDSR, 2, 4, 5, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS] =
            NOR_CMD(OCMD_WRSR, OCMD_WRSR, 2, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE] =
            NOR_CMD(OCMD_WREN, OCMD_WREN, 2, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE] =
            NOR_CMD(OCMD_WRDI, OCMD_WRDI, 2, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_ID] =
            NOR_CMD(OCMD_RDID, OCMD_RDID, 2, 4, 5, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP] =
            NOR_CMD(OCMD_RDSFDP, OCMD_RDSFDP, 2, 4, 20, 0),
        [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] =
            NOR_CMD(OCMD_SE, OCMD_SE, 2, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] =
            NOR_CMD(OP_NONE, OP_NONE, 0, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] =
            NOR_CMD(OCMD_BE, OCMD_BE, 2, 4, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE] =
            NOR_CMD(OCMD_CE_ALT, OCMD_CE_ALT, 2, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE] =
            NOR_CMD(OCMD_RESET_EN, OCMD_RESET_EN, 2, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY] =
            NOR_CMD(OCMD_RESET_MEM, OCMD_RESET_MEM, 2, 0, 0, 0),
        [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2] =
            NOR_CMD(OCMD_RD_CFGREG2, OCMD_RD_CFGREG2, 2, 4, 5, 0),
        [AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2] =
            NOR_CMD(OCMD_WR_CFGREG2, OCMD_WR_CFGREG2, 2, 4, 0, 0),
    },
};

static const am_devices_xspi_nor_jedec_cmd_profile_t g_sJEDECCmdProfiles[] =
{
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_READ_FAST, OP_PP, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_DUAL_IO_READ, OP_PP_1_1_2, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_QUAD_IO_READ, OP_PP_1_4_4, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_QUAD_READ, OP_PP_1_1_4, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_QUAD_IO_READ, OP_PP_1_4_4, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OCMD_RD, OCMD_PP, 6u, false,
                                        true, true, true, 4u, true, 4u),
    },
    //mirrors i25wx064
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_OCTA_1_1_8_READ, OP_PP_1_1_8, 8u, false,
                                        false, false, false, 0u, false, 0u),
    },
    //mirrors i25wx064
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OP_OCTA_1_8_8_READ, OP_PP_1_8_8, 16u, false,
                                        false, false, false, 0u, false, 0u),
    },
    {
        .eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .sCmdInfo  = NOR_JEDEC_CMD_INFO(OCMD_DTR_RD, OCMD_PP, 6u, true,
                                        true, true, true, 5u, true, 5u),
    },
};

const am_devices_xspi_nor_cmd_set_t *
am_devices_xspi_nor_cmd_set_get(am_devices_xspi_nor_protocol_e eProtocol)
{
    switch (eProtocol)
    {
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S:
            return &g_sProto1S;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S:
            return NULL;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S:
            return NULL;
        case AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S:
            return &g_sProto2S;
        case AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S:
            return &g_sProto4S;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S:
            return &g_sProto1S1S4S;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S:
            return &g_sProto1S4S4S;
        case AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S:
            return NULL;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S:
            return &g_sProto1S1S8S;
        case AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S:
            return &g_sProto1S8S8S;
        case AM_DEVICES_XSPI_NOR_PROTO_2S_2D_2D:
            return NULL;
        case AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D:
            return NULL;
        case AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D:
            return &g_sProto8D;
        default:
            break;
    }

    return NULL;
}

const am_devices_xspi_nor_jedec_cmd_profile_t *
am_devices_xspi_nor_jedec_cmd_profile_get(am_devices_xspi_nor_protocol_e eProtocol)
{
    for (uint32_t i = 0; i < sizeof(g_sJEDECCmdProfiles) / sizeof(g_sJEDECCmdProfiles[0]); i++)
    {
        if (g_sJEDECCmdProfiles[i].eProtocol == eProtocol)
        {
            return &g_sJEDECCmdProfiles[i];
        }
    }

    return NULL;
}

am_devices_xspi_nor_protocol_e
am_devices_xspi_nor_protocol_from_hal(am_hal_xspimc_io_mode_e eIOMode,
                                      am_hal_xspimc_data_rate_e eDataRate)
{
    am_devices_xspi_nor_protocol_e eProtocol =
        AM_DEVICES_XSPI_NOR_PROTO_INVALID;

    switch (eDataRate)
    {
        case AM_HAL_XSPIMC_DATA_RATE_SINGLE:
            switch (eIOMode)
            {
                case AM_HAL_XSPIMC_IO_SERIAL:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S;
                    break;
                case AM_HAL_XSPIMC_IO_1_1_2:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S;
                    break;
                case AM_HAL_XSPIMC_IO_1_2_2:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S;
                    break;
                case AM_HAL_XSPIMC_IO_DUAL:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S;
                    break;
                case AM_HAL_XSPIMC_IO_QUAD:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S;
                    break;
                case AM_HAL_XSPIMC_IO_1_1_4:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S;
                    break;
                case AM_HAL_XSPIMC_IO_1_4_4:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S;
                    break;
                case AM_HAL_XSPIMC_IO_OCTAL:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S;
                    break;
                case AM_HAL_XSPIMC_IO_1_1_8:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S;
                    break;
                case AM_HAL_XSPIMC_IO_1_8_8:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S;
                    break;
                default:
                    break;
            }
            break;

        case AM_HAL_XSPIMC_DATA_RATE_S_S_D:
            switch (eIOMode)
            {
                case AM_HAL_XSPIMC_IO_1_1_2:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2D;
                    break;
                case AM_HAL_XSPIMC_IO_1_1_4:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4D;
                    break;
                case AM_HAL_XSPIMC_IO_1_1_8:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8D;
                    break;
                default:
                    break;
            }
            break;

        case AM_HAL_XSPIMC_DATA_RATE_S_D_D:
            switch (eIOMode)
            {
                case AM_HAL_XSPIMC_IO_1_2_2:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D;
                    break;
                case AM_HAL_XSPIMC_IO_1_4_4:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D;
                    break;
                case AM_HAL_XSPIMC_IO_1_8_8:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_8D_8D;
                    break;
                case AM_HAL_XSPIMC_IO_DUAL:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2D_2D;
                    break;
                case AM_HAL_XSPIMC_IO_QUAD:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D;
                    break;
                default:
                    break;
            }
            break;

        case AM_HAL_XSPIMC_DATA_RATE_DUAL:
            switch (eIOMode)
            {
                case AM_HAL_XSPIMC_IO_OCTAL:
                    eProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D;
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    return eProtocol;
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
