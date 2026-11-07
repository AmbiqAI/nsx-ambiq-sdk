//*****************************************************************************
//
//! @file am_devices_xspi_nor_mx25um51245g.c
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

#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor_mx25um51245g.h"

#if AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G

#define MX25UM_CMD_WRCR2                 AM_DEVICES_XSPI_NOR_OP_WR_CFGREG2
#define MX25UM_CR2_ADDR_MODE             0x00000000u
#define MX25UM_CR2_ADDR_DUMMY            0x00000300u
#define MX25UM_MODE_SPI                  0x00u
#define MX25UM_MODE_SOPI                 0x01u
#define MX25UM_MODE_DOPI                 0x02u
#define MX25UM_DUMMY_CYCLE_6             0x07u

static const am_devices_xspi_nor_erase_type_t g_sMX25UMEraseTypes[] =
{
    { 64u * 1024u, AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K },
    { 4u * 1024u,  AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K  },
};

static const uint8_t g_ui8MX25UMModeSPI[]  = {MX25UM_MODE_SPI};
static const uint8_t g_ui8MX25UMModeSOPI[] = {MX25UM_MODE_SOPI};
static const uint8_t g_ui8MX25UMModeDOPI[] = {MX25UM_MODE_DOPI};
static const uint8_t g_ui8MX25UMDummy[]    = {MX25UM_DUMMY_CYCLE_6};

#define MX25UM_WREN_SWITCH_OP(_cmd, _cmd_len)  \
        {                                      \
            .ui16Cmd      = (_cmd),            \
            .ui8CmdLen    = (_cmd_len),        \
            .ui8AddrLen   = 0u,                \
            .ui32Addr     = 0u,                \
            .pui8Data     = NULL,              \
            .ui32NumBytes = 0u,                \
            .ui8TxDummy   = 0u,                \
        }

static const am_devices_xspi_nor_switch_op_t g_sMX25UMSerialOps[] =
{
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OCMD_WREN, 2u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_MODE,
        .pui8Data     = g_ui8MX25UMModeSPI,
        .ui32NumBytes = sizeof(g_ui8MX25UMModeSPI),
        .ui8TxDummy   = 0u,
    },
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OCMD_WREN, 2u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_DUMMY,
        .pui8Data     = g_ui8MX25UMDummy,
        .ui32NumBytes = sizeof(g_ui8MX25UMDummy),
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sMX25UMSOPIOps[] =
{
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OP_WREN, 1u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_MODE,
        .pui8Data     = g_ui8MX25UMModeSOPI,
        .ui32NumBytes = sizeof(g_ui8MX25UMModeSOPI),
        .ui8TxDummy   = 0u,
    },
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OP_WREN, 1u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_DUMMY,
        .pui8Data     = g_ui8MX25UMDummy,
        .ui32NumBytes = sizeof(g_ui8MX25UMDummy),
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_switch_op_t g_sMX25UMDOPIOps[] =
{
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OP_WREN, 1u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_MODE,
        .pui8Data     = g_ui8MX25UMModeDOPI,
        .ui32NumBytes = sizeof(g_ui8MX25UMModeDOPI),
        .ui8TxDummy   = 0u,
    },
    MX25UM_WREN_SWITCH_OP(AM_DEVICES_XSPI_NOR_OP_WREN, 1u),
    {
        .ui16Cmd      = MX25UM_CMD_WRCR2,
        .ui8CmdLen    = 1u,
        .ui8AddrLen   = 4u,
        .ui32Addr     = MX25UM_CR2_ADDR_DUMMY,
        .pui8Data     = g_ui8MX25UMDummy,
        .ui32NumBytes = sizeof(g_ui8MX25UMDummy),
        .ui8TxDummy   = 0u,
    },
};

static const am_devices_xspi_nor_mode_switch_t g_sMX25UMModeSwitches[] =
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
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        .pEnterOps       = g_sMX25UMSOPIOps,
        .ui32NumEnterOps = sizeof(g_sMX25UMSOPIOps) / sizeof(g_sMX25UMSOPIOps[0]),
        .pExitOps        = g_sMX25UMSerialOps,
        .ui32NumExitOps  = sizeof(g_sMX25UMSerialOps) / sizeof(g_sMX25UMSerialOps[0]),
    },
    {
        .eProtocol       = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
        .eModeEnableReq  = AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
        .pEnterOps       = g_sMX25UMDOPIOps,
        .ui32NumEnterOps = sizeof(g_sMX25UMDOPIOps) / sizeof(g_sMX25UMDOPIOps[0]),
        .pExitOps        = g_sMX25UMSerialOps,
        .ui32NumExitOps  = sizeof(g_sMX25UMSerialOps) / sizeof(g_sMX25UMSerialOps[0]),
    },
};

static const am_devices_xspi_nor_quirks_t g_sMX25UMQuirks =
{
    .soft_reset    = NULL,
    .post_reset    = NULL,
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
// Suspend/resume latencies and resume-to-suspend spacing are from the
// MX25UM51245G v1.6 datasheet, AC Characteristics:
//   tESL (erase suspend latency) = 25 us, tPSL (program suspend latency) = 25 us
//   tPRS (program resume -> next suspend) = 0.3..100 us
//   tERS (erase resume -> next suspend)  = 0.3..400 us
// Worst-case busy times:
//   tPP (page program)   = 0.75 ms = 750 us
//   tSE (sector erase 4K) = 400 ms   = 400,000 us
//   tBE (block erase 64K) = 2000 ms  = 2,000,000 us
// Chip erase (tCE = 300 s) is excluded: it cannot be encoded in the 32-bit
// holdoff cycle field at typical bus clocks; full-device erase decomposes
// into RWW-eligible block erases instead.
//
static const am_devices_xspi_nor_rww_op_t g_sMX25UMRWWOps1S[] =
{
    { AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM,      750u },
    { AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K,   400000u },
    { AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K,   2000000u },
};

static const am_devices_xspi_nor_rww_cfg_t g_sMX25UMRWWCfgs[] =
{
    {
        .eProtocol                 = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S,
        .eSuspendCmd               = AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
        .eResumeCmd                = AM_DEVICES_XSPI_NOR_CMD_RESUME,
        .ui32SuspendTimeUs         = 30u,
        .ui32ResumeTimeUs          = 30u,
        .ui32SuspendToResumeTimeUs = 10u,
        .ui32ResumeToSuspendTimeUs = 400u,
        .ui32HoldoffGuardTimeUs    = 100u,
        .pOperations               = g_sMX25UMRWWOps1S,
        .ui32NumOperations         = sizeof(g_sMX25UMRWWOps1S) /
                                     sizeof(g_sMX25UMRWWOps1S[0]),
    },
};

const am_devices_xspi_nor_device_t g_sXSPINORMX25UM51245G =
{
    .pName            = "MX25UM51245G",
    .ui8VendorId      = 0xC2u,
    .ui8DeviceId      = 0x80u,
    .ui32DeviceSizeKb = AM_DEVICES_XSPI_NOR_MX25UM51245G_FLASH_SIZE_KB,
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
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S) |
        AM_DEVICES_XSPI_NOR_PROTO_BIT(AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D),
    .ui8JedecIdReadLen       = 3u,
    .bSupport4ByteAddress    = true,
    .bUse4ByteOpcodes        = true,
    .eEnter4ByteMethod       = AM_DEVICES_XSPI_NOR_ENTER_4BYTE_NONE,
    .pEraseTypes             = g_sMX25UMEraseTypes,
    .ui32NumEraseTypes       = sizeof(g_sMX25UMEraseTypes) / sizeof(g_sMX25UMEraseTypes[0]),
    .pCmdSets                = NULL,
    .ui32NumCmdSets          = 0u,
    .pJEDECCmdProfiles       = NULL,
    .ui32NumJEDECCmdProfiles = 0u,
    .pModeSwitches           = g_sMX25UMModeSwitches,
    .ui32NumModeSwitches     = sizeof(g_sMX25UMModeSwitches) / sizeof(g_sMX25UMModeSwitches[0]),
    .pRWWCfgs                = g_sMX25UMRWWCfgs,
    .ui32NumRWWCfgs          = sizeof(g_sMX25UMRWWCfgs) / sizeof(g_sMX25UMRWWCfgs[0]),
    .pQuirks                 = &g_sMX25UMQuirks,
};

//*****************************************************************************
//
//! @brief Internal helper for xSPI NOR flow.
//
//*****************************************************************************
static uint32_t
mx25um_config(am_device_xspi_periph_handle_t *pFlash,
              const am_devices_xspi_norflash_cfg_t *pConfig)
{
    return am_devices_xspi_nor_config(pFlash, pConfig, &g_sXSPINORMX25UM51245G);
}

//*****************************************************************************
//
//! @brief Internal helper for xSPI NOR flow.
//
//*****************************************************************************
static uint32_t
mx25um_power(am_device_xspi_periph_handle_t *pFlash,
             am_devices_xspi_power_e ePowerMode,
             const am_devices_xspi_nor_power_cfg_t *pPowerCfg)
{
    return am_devices_xspi_nor_power_control(pFlash, ePowerMode, pPowerCfg);
}

const am_device_xspi_func_t apiMX25UM51245G =
{
    .device_init   = am_devices_xspi_nor_init,
    .device_deinit = am_devices_xspi_nor_deinit,
    .device_config = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                   const am_devices_xspi_config_t *,
                                   ...)) mx25um_config,
    .device_get_info      = am_devices_xspi_nor_get_info,
    .device_write         = am_devices_xspi_nor_write,
    .device_read          = am_devices_xspi_nor_read,
    .device_erase         = am_devices_xspi_nor_erase,
    .device_set_xfer_mode = am_devices_xspi_nor_set_xfer_mode,
    .device_power_ctrl    = (uint32_t (*)(am_device_xspi_periph_handle_t *,
                                          am_devices_xspi_power_e,
                                          ...)) mx25um_power,
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
