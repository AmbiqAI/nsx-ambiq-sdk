//*****************************************************************************
//
//! @file am_devices_xspi_nor.h
//!
//! @brief Generic xSPI NOR flash definitions.
//!
//! @addtogroup devices_xspi_nor Generic xSPI NOR Flash
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

#ifndef AM_DEVICES_XSPI_NOR_H
#define AM_DEVICES_XSPI_NOR_H

#include "am_devices_xspi.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Build-time device enables
//! @{
//
//*****************************************************************************
#ifndef AM_DEVICES_XSPI_NOR_ENABLE_ATXP032
#define AM_DEVICES_XSPI_NOR_ENABLE_ATXP032      1
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064
#define AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064    1
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G
#define AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G 1
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_W25Q25
#define AM_DEVICES_XSPI_NOR_ENABLE_W25Q25      1
#endif
//! @}

//*****************************************************************************
//
//! @name AIP read-while-write opt-in
//! @{
//
//*****************************************************************************
#ifndef AM_DEVICES_XSPI_NOR_ENABLE_AIP_READ
#define AM_DEVICES_XSPI_NOR_ENABLE_AIP_READ    0
#endif
//! @}

//*****************************************************************************
//
//! @name Generic NOR constants
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_NOR_TIMEOUT_US        AM_HAL_XSPIMC_TIMEOUT_MAX
#define AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ 25000000u

#define AM_DEVICES_XSPI_NOR_RAW_RAM_SIZE      128u
#define AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE   (AM_DEVICES_XSPI_NOR_RAW_RAM_SIZE)

#define AM_DEVICES_XSPI_NOR_MAX_ERASE_TYPES   4u
#define AM_DEVICES_XSPI_NOR_PROTO_BIT(_proto) \
        (1UL << ((uint32_t)(_proto)))
#define AM_DEVICES_XSPI_NOR_CMD_BIT(_cmd)      \
        (1UL << ((uint32_t)(_cmd)))
//! @}

//*****************************************************************************
//
//! @name Generic NOR JEDEC command opcodes
//! @{
//
//*****************************************************************************
#define AM_DEVICES_XSPI_NOR_OP_NONE               0x0000u

#define AM_DEVICES_XSPI_NOR_OP_WRSR               0x01u
#define AM_DEVICES_XSPI_NOR_OP_RDSR               0x05u
#define AM_DEVICES_XSPI_NOR_OP_WRSR2              0x31u
#define AM_DEVICES_XSPI_NOR_OP_RDSR2              0x35u
#define AM_DEVICES_XSPI_NOR_OP_WRSR2_ALT          0x3Eu
#define AM_DEVICES_XSPI_NOR_OP_RDSR2_ALT          0x3Fu
#define AM_DEVICES_XSPI_NOR_OP_RDOER              0x65u
#define AM_DEVICES_XSPI_NOR_OP_WRSR3              0x11u
#define AM_DEVICES_XSPI_NOR_OP_RDSR3              0x15u
#define AM_DEVICES_XSPI_NOR_OP_RDCR               0x15u

#define AM_DEVICES_XSPI_NOR_OP_READ_FAST          0x0Bu
#define AM_DEVICES_XSPI_NOR_OP_READ_FAST_4B       0x0Cu
#define AM_DEVICES_XSPI_NOR_OP_READ_4B            0x13u
#define AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ       0xBBu
#define AM_DEVICES_XSPI_NOR_OP_DUAL_IO_READ_4B    0xBCu
#define AM_DEVICES_XSPI_NOR_OP_QUAD_READ          0x6Bu
#define AM_DEVICES_XSPI_NOR_OP_QUAD_READ_4B       0x6Cu
#define AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ       0xEBu
#define AM_DEVICES_XSPI_NOR_OP_QUAD_IO_READ_4B    0xECu
#define AM_DEVICES_XSPI_NOR_OP_1_1_8_READ         0x8Bu
#define AM_DEVICES_XSPI_NOR_OP_1_1_8_READ_4B      0x7Cu
#define AM_DEVICES_XSPI_NOR_OP_1_8_8_READ         0xCBu
#define AM_DEVICES_XSPI_NOR_OP_1_8_8_READ_4B      0xCCu
#define AM_DEVICES_XSPI_NOR_OP_1_8_8_READ_DTR_4B  0xFDu

#define AM_DEVICES_XSPI_NOR_OP_WREN               0x06u
#define AM_DEVICES_XSPI_NOR_OP_WRDI               0x04u
#define AM_DEVICES_XSPI_NOR_OP_PP                 0x02u
#define AM_DEVICES_XSPI_NOR_OP_PP_4B              0x12u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_1_2           0xA2u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_1_4           0x32u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_1_4_4B        0x34u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_4_4           0x38u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_4_4_4B        0x3Eu
#define AM_DEVICES_XSPI_NOR_OP_PP_1_1_8           0x82u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_1_8_4B        0x84u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_8_8           0xC2u
#define AM_DEVICES_XSPI_NOR_OP_PP_1_8_8_4B        0x8Eu

#define AM_DEVICES_XSPI_NOR_OP_RDID               0x9Fu
#define AM_DEVICES_XSPI_NOR_OP_RDSFDP             0x5Au
#define AM_DEVICES_XSPI_NOR_OP_SE                 0x20u
#define AM_DEVICES_XSPI_NOR_OP_SE_4B              0x21u
#define AM_DEVICES_XSPI_NOR_OP_BE_32K             0x52u
#define AM_DEVICES_XSPI_NOR_OP_BE_32K_4B          0x5Cu
#define AM_DEVICES_XSPI_NOR_OP_BE                 0xD8u
#define AM_DEVICES_XSPI_NOR_OP_BE_4B              0xDCu
#define AM_DEVICES_XSPI_NOR_OP_CE                 0xC7u
#define AM_DEVICES_XSPI_NOR_OP_CE_ALT             0x60u
#define AM_DEVICES_XSPI_NOR_OP_ENTER_4BYTE        0xB7u
#define AM_DEVICES_XSPI_NOR_OP_EXIT_4BYTE         0xE9u
#define AM_DEVICES_XSPI_NOR_OP_RESET_EN           0x66u
#define AM_DEVICES_XSPI_NOR_OP_RESET_MEM          0x99u
#define AM_DEVICES_XSPI_NOR_OP_SUSPEND            0x75u
#define AM_DEVICES_XSPI_NOR_OP_RESUME             0x7Au
#define AM_DEVICES_XSPI_NOR_OP_WR_CFGREG2         0x72u
#define AM_DEVICES_XSPI_NOR_OP_RD_CFGREG2         0x71u
#define AM_DEVICES_XSPI_NOR_OP_WR_VCFGREG         0x81u
#define AM_DEVICES_XSPI_NOR_OP_RD_VCFGREG         0x85u
#define AM_DEVICES_XSPI_NOR_OP_RDFLSR             0x70u

#define AM_DEVICES_XSPI_NOR_OCMD_RD               0xEC13u
#define AM_DEVICES_XSPI_NOR_OCMD_DTR_RD           0xEE11u
#define AM_DEVICES_XSPI_NOR_OCMD_PP               0x12EDu
#define AM_DEVICES_XSPI_NOR_OCMD_RDSR             0x05FAu
#define AM_DEVICES_XSPI_NOR_OCMD_WRSR             0x01FEu
#define AM_DEVICES_XSPI_NOR_OCMD_WREN             0x06F9u
#define AM_DEVICES_XSPI_NOR_OCMD_WRDI             0x04FBu
#define AM_DEVICES_XSPI_NOR_OCMD_RDID             0x9F60u
#define AM_DEVICES_XSPI_NOR_OCMD_RDSFDP           0x5AA5u
#define AM_DEVICES_XSPI_NOR_OCMD_SE               0x21DEu
#define AM_DEVICES_XSPI_NOR_OCMD_BE               0xDC23u
#define AM_DEVICES_XSPI_NOR_OCMD_CE               0xC738u
#define AM_DEVICES_XSPI_NOR_OCMD_CE_ALT           0x609Fu
#define AM_DEVICES_XSPI_NOR_OCMD_RESET_EN         0x6699u
#define AM_DEVICES_XSPI_NOR_OCMD_RESET_MEM        0x9966u
#define AM_DEVICES_XSPI_NOR_OCMD_WR_CFGREG2       0x728Du
#define AM_DEVICES_XSPI_NOR_OCMD_RD_CFGREG2       0x718Eu

#define AM_DEVICES_XSPI_NOR_OCMD_EXT_SAME(_op) \
        ((uint16_t)((((uint16_t)(_op)) << 8) | ((uint16_t)(_op))))
#define AM_DEVICES_XSPI_NOR_OCMD_EXT_INVERSE(_op) \
        ((uint16_t)((((uint16_t)(_op)) << 8) | ((uint16_t)(~((uint16_t)(_op))) & 0x00FFu)))

#define AM_DEVICES_XSPI_NOR_SR_WIP                0x01u
#define AM_DEVICES_XSPI_NOR_FSR_READY             0x80u
//! @}

//*****************************************************************************
//
//! @brief Protocol (line mode + data rate) identifiers.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S = 0,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2D,
    AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4D,
    AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8D,
    AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S,
    AM_DEVICES_XSPI_NOR_PROTO_1S_8D_8D,
    AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S,
    AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S,
    AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S,
    AM_DEVICES_XSPI_NOR_PROTO_2S_2D_2D,
    AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D,
    AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D,
    AM_DEVICES_XSPI_NOR_PROTO_MAX,
    AM_DEVICES_XSPI_NOR_PROTO_INVALID = 0xFF
} am_devices_xspi_nor_protocol_e;

typedef struct
{
    am_devices_xspi_nor_protocol_e eProtocol;
    uint32_t                       ui32Freq;
    bool                           bDQS;
} am_devices_xspi_nor_protocol_cfg_t;

#include "am_devices_xspi_nor_quirks.h"

//*****************************************************************************
//
//! @brief Generic NOR command identifiers.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_XSPI_NOR_CMD_READ = 0,
    AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM,
    AM_DEVICES_XSPI_NOR_CMD_READ_STATUS,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS,
    AM_DEVICES_XSPI_NOR_CMD_READ_STATUS2,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS2,
    AM_DEVICES_XSPI_NOR_CMD_READ_STATUS3,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS3,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE,
    AM_DEVICES_XSPI_NOR_CMD_READ_ID,
    AM_DEVICES_XSPI_NOR_CMD_READ_SFDP,
    AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K,
    AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K,
    AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K,
    AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE,
    AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE,
    AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE,
    AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE,
    AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY,
    AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG,
    AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2,
    AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2,
    AM_DEVICES_XSPI_NOR_CMD_SUSPEND,
    AM_DEVICES_XSPI_NOR_CMD_RESUME,
    AM_DEVICES_XSPI_NOR_CMD_MAX
} am_devices_xspi_nor_cmd_e;

//*****************************************************************************
//
//! @brief Per-command transport attributes.
//
//*****************************************************************************
typedef struct
{
    uint16_t ui16Cmd;
    uint16_t ui16Cmd4Byte;
    uint8_t  ui8CmdLen;
    uint8_t  ui8AddrLen;
    uint8_t  ui8RxDummy;
    uint8_t  ui8TxDummy;
    uint8_t  ui8ModeBitLen;
    uint32_t ui32ModeBits;
} am_devices_xspi_nor_cmd_info_t;

//*****************************************************************************
//
//! @brief Command set for one protocol.
//
//*****************************************************************************
typedef struct
{
    am_devices_xspi_nor_protocol_e eProtocol;
    am_hal_xspimc_io_mode_e        eIOMode;
    am_hal_xspimc_data_rate_e      eDataRate;
    am_devices_xspi_nor_cmd_info_t asCmdInfo[AM_DEVICES_XSPI_NOR_CMD_MAX];
} am_devices_xspi_nor_cmd_set_t;

typedef struct
{
    am_devices_xspi_nor_protocol_e eSourceProtocol;
    am_devices_xspi_nor_protocol_e eFallbackProtocol;
    uint32_t                       ui32CmdMask;
} am_devices_xspi_nor_cmd_fallback_t;

typedef struct
{
    uint32_t                  ui32Size;
    am_devices_xspi_nor_cmd_e eCmd;
} am_devices_xspi_nor_erase_type_t;

typedef enum
{
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_NONE = 0,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_B7,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_06_B7,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_EAR,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_BANK,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_NVCR,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_DEDICATED_4B_OPCODES,
    AM_DEVICES_XSPI_NOR_ENTER_4BYTE_ALWAYS_4BYTE,
} am_devices_xspi_nor_enter_4byte_e;

typedef enum
{
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_AUTO = 0,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_SPI,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_DTR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_STR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_DTR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_STR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_DTR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_STR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_DTR,
    AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DEVICE,
} am_devices_xspi_nor_mode_domain_e;

typedef enum
{
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE = 0,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_NONE,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V1,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S1B6,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B7,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V4,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V5,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_NONE,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_S2B3,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_OPS,
    AM_DEVICES_XSPI_NOR_MODE_ENABLE_DEVICE_SPECIFIC,
} am_devices_xspi_nor_mode_enable_e;

typedef struct
{
    uint16_t                          ui16ReadCmd;
    uint16_t                          ui16PageProgramCmd;
    uint8_t                           ui8ReadDummyCycles;
    bool                              bUses4ByteAddr;
    bool                              bSFDPAddr4;
    bool                              bSFDPDummy20;
    bool                              bRDSRAddr4;
    uint8_t                           ui8RDSRDummy;
    bool                              bRDIDAddr4;
    uint8_t                           ui8RDIDDummy;
    am_devices_xspi_nor_mode_domain_e eDeviceModeDomain;
    am_devices_xspi_nor_mode_enable_e eModeEnableReq;
} am_devices_xspi_nor_jedec_cmd_info_t;

typedef struct
{
    am_devices_xspi_nor_protocol_e       eProtocol;
    am_devices_xspi_nor_jedec_cmd_info_t sCmdInfo;
} am_devices_xspi_nor_jedec_cmd_profile_t;

typedef struct
{
    uint16_t      ui16Cmd;
    uint8_t       ui8CmdLen;
    uint8_t       ui8AddrLen;
    uint32_t      ui32Addr;
    const uint8_t *pui8Data;
    uint32_t      ui32NumBytes;
    uint8_t       ui8TxDummy;
} am_devices_xspi_nor_switch_op_t;

typedef struct
{
    am_devices_xspi_nor_protocol_e        eProtocol;
    am_devices_xspi_nor_mode_enable_e     eModeEnableReq;
    bool                                  bDisableOnExit;
    const am_devices_xspi_nor_switch_op_t *pEnterOps;
    uint32_t                              ui32NumEnterOps;
    const am_devices_xspi_nor_switch_op_t *pExitOps;
    uint32_t                              ui32NumExitOps;
} am_devices_xspi_nor_mode_switch_t;

//*****************************************************************************
//
//! @brief One long-running program/erase operation eligible for read-while-write.
//!
//! @ingroup devices_xspi_nor
//!
//! Pairs a logical command identifier with its worst-case busy time. Only
//! actual program/erase commands supported for RWW in a protocol appear in a
//! profile's operation table. Times are in microseconds.
//
//*****************************************************************************
typedef struct
{
    am_devices_xspi_nor_cmd_e eCmd;
    uint32_t                  ui32MaxBusyTimeUs;
} am_devices_xspi_nor_rww_op_t;

//*****************************************************************************
//
//! @brief Protocol-specific read-while-write profile.
//!
//! @ingroup devices_xspi_nor
//!
//! Immutable, wrapper-local profile describing suspend/resume behavior for one
//! configured target protocol. The presence of a profile whose @p eProtocol
//! matches the configured target protocol is the sole capability declaration;
//! a wrapper with no RWW support leaves @ref am_devices_xspi_nor_device_t
//! ::pRWWCfgs NULL and ::ui32NumRWWCfgs zero.
//!
//! Transport I/O mode and data rate are deliberately absent: they are derived
//! from the resolved target @ref am_devices_xspi_nor_cmd_set_t so transport
//! data has one source of truth. Suspend/resume fields carry logical
//! ::am_devices_xspi_nor_cmd_e values, never raw opcodes.
//!
//! All time fields are in microseconds. @p pOperations points to a
//! wrapper-local static const array; the generic driver scans it in place and
//! never copies the full wrapper array into runtime state.
//
//*****************************************************************************
typedef struct
{
    am_devices_xspi_nor_protocol_e     eProtocol;
    am_devices_xspi_nor_cmd_e          eSuspendCmd;
    am_devices_xspi_nor_cmd_e          eResumeCmd;
    uint32_t                           ui32SuspendTimeUs;
    uint32_t                           ui32ResumeTimeUs;
    uint32_t                           ui32SuspendToResumeTimeUs;
    uint32_t                           ui32ResumeToSuspendTimeUs;
    uint32_t                           ui32HoldoffGuardTimeUs;
    const am_devices_xspi_nor_rww_op_t *pOperations;
    uint32_t                           ui32NumOperations;
} am_devices_xspi_nor_rww_cfg_t;

//*****************************************************************************
//
//! @brief Static device descriptor.
//
//*****************************************************************************
typedef struct
{
    const char                                    *pName;
    uint8_t                                       ui8VendorId;
    uint8_t                                       ui8DeviceId;
    uint32_t                                      ui32DeviceSizeKb;
    uint32_t                                      ui32PageSize;
    uint32_t                                      ui32DefaultFreq;
    uint32_t                                      ui32ControlFreq;
    am_hal_xspimc_ce_timing                       sCETiming;
    am_devices_xspi_nor_protocol_e                eDefaultProtocol;
    uint32_t                                      ui32SupportedProtoMask;
    uint8_t                                       ui8JedecIdReadLen;
    bool                                          bSupport4ByteAddress;
    bool                                          bUse4ByteOpcodes;
    am_devices_xspi_nor_enter_4byte_e             eEnter4ByteMethod;
    const am_devices_xspi_nor_erase_type_t        *pEraseTypes;
    uint32_t                                      ui32NumEraseTypes;
    const am_devices_xspi_nor_cmd_set_t           *pCmdSets;
    uint32_t                                      ui32NumCmdSets;
    const am_devices_xspi_nor_cmd_fallback_t      *pCmdFallbacks;
    uint32_t                                      ui32NumCmdFallbacks;
    const am_devices_xspi_nor_jedec_cmd_profile_t *pJEDECCmdProfiles;
    uint32_t                                      ui32NumJEDECCmdProfiles;
    const am_devices_xspi_nor_mode_switch_t       *pModeSwitches;
    uint32_t                                      ui32NumModeSwitches;
    const am_devices_xspi_nor_rww_cfg_t           *pRWWCfgs;
    uint32_t                                      ui32NumRWWCfgs;
    const am_devices_xspi_nor_quirks_t            *pQuirks;
} am_devices_xspi_nor_device_t;

typedef struct
{
    uint32_t ui32Reserved;
} am_devices_xspi_nor_power_cfg_t;

//*****************************************************************************
//
//! @brief Core generic NOR APIs used by device-specific wrappers.
//
//*****************************************************************************
extern uint32_t am_devices_xspi_nor_init(am_device_xspi_periph_handle_t *pFlash);
extern uint32_t am_devices_xspi_nor_deinit(am_device_xspi_periph_handle_t *pFlash);
extern uint32_t am_devices_xspi_nor_config(am_device_xspi_periph_handle_t *pFlash,
                                           const am_devices_xspi_norflash_cfg_t *pConfig,
                                           const am_devices_xspi_nor_device_t *pDevice);
extern uint32_t am_devices_xspi_nor_get_info(am_device_xspi_periph_handle_t *pFlash,
                                             am_devices_xspi_info_t *pInfo);
extern uint32_t am_devices_xspi_nor_write(am_device_xspi_periph_handle_t *pFlash,
                                          uint32_t ui32Addr,
                                          uint32_t ui32Size,
                                          void *pData);
extern uint32_t am_devices_xspi_nor_read(am_device_xspi_periph_handle_t *pFlash,
                                         uint32_t ui32Addr,
                                         uint32_t ui32Size,
                                         void *pData);
extern uint32_t am_devices_xspi_nor_erase(am_device_xspi_periph_handle_t *pFlash,
                                          uint32_t ui32Addr,
                                          uint32_t ui32Size);
extern uint32_t am_devices_xspi_nor_power_control(am_device_xspi_periph_handle_t *pFlash,
                                                  am_devices_xspi_power_e ePowerMode,
                                                  const am_devices_xspi_nor_power_cfg_t *pPowerCfg);
extern uint32_t am_devices_xspi_nor_mem_map_config(am_device_xspi_periph_handle_t *pFlash,
                                                   const am_devices_xspi_memmap_cfg_t *pCfg);
extern uint32_t am_devices_xspi_nor_timing_apply(am_device_xspi_periph_handle_t *pFlash,
                                                 const am_devices_xspi_timing_t *pCfg);

//*****************************************************************************
//
//! @brief Optional helper APIs used by device-specific quirks.
//
//*****************************************************************************
extern uint32_t am_devices_xspi_nor_command_write(am_device_xspi_periph_handle_t *pFlash,
                                                  uint16_t ui16Cmd,
                                                  uint8_t ui8CmdLen,
                                                  uint8_t ui8AddrLen,
                                                  uint32_t ui32Addr,
                                                  const uint8_t *pData,
                                                  uint32_t ui32NumBytes,
                                                  uint8_t ui8TxDummy);
extern uint32_t am_devices_xspi_nor_command_read(am_device_xspi_periph_handle_t *pFlash,
                                                 uint16_t ui16Cmd,
                                                 uint8_t ui8CmdLen,
                                                 uint8_t ui8AddrLen,
                                                 uint32_t ui32Addr,
                                                 uint8_t *pData,
                                                 uint32_t ui32NumBytes,
                                                 uint8_t ui8RxDummy);
extern uint32_t am_devices_xspi_nor_write_enable(am_device_xspi_periph_handle_t *pFlash);
extern uint32_t am_devices_xspi_nor_wait_ready(am_device_xspi_periph_handle_t *pFlash,
                                               uint32_t ui32TimeoutUs);
extern uint32_t am_devices_xspi_nor_read_sfdp(am_device_xspi_periph_handle_t *pFlash,
                                              uint32_t ui32Addr,
                                              uint8_t *pData,
                                              uint32_t ui32NumBytes);
extern uint32_t am_devices_xspi_nor_read_jedec_id(am_device_xspi_periph_handle_t *pFlash,
                                                  uint8_t *pRawId,
                                                  uint32_t ui32BufLen);
extern uint32_t am_devices_xspi_nor_set_xfer_mode(am_device_xspi_periph_handle_t *pFlash,
                                                  am_hal_xspimc_xfer_mode_t eXferMode);
extern uint32_t am_devices_xspi_nor_apply_protocol(am_device_xspi_periph_handle_t *pFlash,
                                                   am_devices_xspi_nor_protocol_e eProtocol,
                                                   uint32_t ui32Freq,
                                                   bool bDQS);
extern uint32_t am_devices_xspi_nor_protocol_cfg_get(am_device_xspi_periph_handle_t *pFlash,
                                                     am_devices_xspi_nor_protocol_cfg_t *pCfg);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_NOR_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
