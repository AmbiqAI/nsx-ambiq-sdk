//*****************************************************************************
//
//! @file am_devices_xspi_nor.c
//!
//! @brief Generic xSPI NOR flash core implementation.
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

#include <string.h>
#include "am_mcu_apollo.h"
#include "am_devices_xspi_nor.h"
#include "am_devices_xspi_nor_cmds.h"
#include "am_bsp.h"

#ifndef XSPI_NOR_MAX_RW_CHUNK
#define XSPI_NOR_MAX_RW_CHUNK      0x20000000u //2^32 / 8
#endif

#define XSPI_NOR_MAX_JEDEC_ID_LEN  16u

typedef struct
{
    void                                          *pBus;
    am_hal_xspimc_dev_id                           sDeviceID;
    am_devices_xspi_hal_config_t                   sHalCfg;
    const am_devices_xspi_nor_device_t            *pDevice;
    const am_devices_xspi_nor_cmd_set_t           *pCmdSet;
    const am_devices_xspi_nor_jedec_cmd_profile_t *pJEDECCmdProfile;
    const am_devices_xspi_nor_rww_cfg_t           *pRWWCfg;
    am_devices_xspi_info_t                         sInfo;
    uint32_t                                       ui32PageSize;
    am_hal_xspimc_xfer_mode_t                      eXferMode;
    am_devices_xspi_nor_protocol_e                 eCurrentProtocol;
    bool                                           bConfigured;
    bool                                           bUse4ByteAddr;
    bool                                           bBypassControlSwitch;
    bool                                           bRWWEnabled;
} xspi_nor_state_t;

typedef struct
{
    bool bRegisterClass;
} am_devices_xspi_nor_cmd_policy_t;

typedef struct
{
    bool                                 bSwitched;
    am_devices_xspi_nor_protocol_e       eProtocol;
    uint32_t                             ui32Freq;
    bool                                 bDQS;
    const am_devices_xspi_nor_cmd_info_t *pCmd;
} xspi_nor_mode_ctx_t;

typedef char xspi_nor_state_size_fits[
        (sizeof(xspi_nor_state_t) <= AM_DEVICES_XSPI_NOR_GLOBAL_RAM_SIZE) ? 1 : -1];

typedef struct
{
    am_hal_xspimc_xfer_dir_e eDir;
    uint16_t                 ui16Cmd;
    uint8_t                  ui8CmdLen;
    uint8_t                  ui8AddrLen;
    uint8_t                  ui8TxDummy;
    uint8_t                  ui8RxDummy;
    uint8_t                  ui8ModeBitLen;
    uint32_t                 ui32ModeBits;
    uint32_t                 ui32Address;
    uint8_t                 *pData;
    uint32_t                 ui32NumBytes;
} xspi_nor_transfer_cfg_t;

typedef union
{
    uint32_t ui32Word;
    uint8_t  aui8[4];
} xspi_nor_word_buf_t;

typedef union
{
    uint32_t aui32[(XSPI_NOR_MAX_JEDEC_ID_LEN + 3u) / 4u];
    uint8_t  aui8[XSPI_NOR_MAX_JEDEC_ID_LEN];
} xspi_nor_jedec_buf_t;

static const am_devices_xspi_nor_cmd_policy_t g_sCmdPolicies[AM_DEVICES_XSPI_NOR_CMD_MAX] =
{
    [AM_DEVICES_XSPI_NOR_CMD_READ]            = { false },
    [AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM]    = { false },
    [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS]     = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS2]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS2]   = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_STATUS3]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_STATUS3]   = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_DISABLE]   = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_ID]         = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_SFDP]       = { true },
    [AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K] = { true },
    [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K] = { true },
    [AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K] = { true },
    [AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE]      = { true },
    [AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE]     = { true },
    [AM_DEVICES_XSPI_NOR_CMD_EXIT_4BYTE]      = { true },
    [AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG]     = { true },
    [AM_DEVICES_XSPI_NOR_CMD_WRITE_CFGREG2]   = { true },
    [AM_DEVICES_XSPI_NOR_CMD_READ_CFGREG2]    = { true },
    [AM_DEVICES_XSPI_NOR_CMD_SUSPEND]         = { true },
    [AM_DEVICES_XSPI_NOR_CMD_RESUME]          = { true },
};

//*****************************************************************************
//
// Transfer Geometry Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Convert HAL IO mode into the active data-line count.
//!
//! @param eIOMode is the HAL IO mode to evaluate.
//!
//! @return Number of active data lines, or 0 when mode is unsupported.
//
//*****************************************************************************
static uint32_t
xspi_nor_data_lines_get(am_hal_xspimc_io_mode_e eIOMode)
{
    switch (eIOMode)
    {
        case AM_HAL_XSPIMC_IO_SERIAL:
            return 1u;

        case AM_HAL_XSPIMC_IO_DUAL:
        case AM_HAL_XSPIMC_IO_1_1_2:
        case AM_HAL_XSPIMC_IO_1_2_2:
            return 2u;

        case AM_HAL_XSPIMC_IO_QUAD:
        case AM_HAL_XSPIMC_IO_1_1_4:
        case AM_HAL_XSPIMC_IO_1_4_4:
            return 4u;

        case AM_HAL_XSPIMC_IO_OCTAL:
        case AM_HAL_XSPIMC_IO_1_1_8:
        case AM_HAL_XSPIMC_IO_1_8_8:
            return 8u;

        case AM_HAL_XSPIMC_IO_HEX:
        case AM_HAL_XSPIMC_IO_8_16_16:
            return 16u;

        default:
            return 0u;
    }
}

//*****************************************************************************
//
//! @brief Get required byte alignment for the active transfer width.
//!
//! @param pState is the NOR runtime state.
//!
//! @return Required byte alignment for valid payload sizes.
//
//*****************************************************************************
static uint32_t
xspi_nor_transfer_align_bytes_get(const xspi_nor_state_t *pState)
{
    uint32_t ui32DataLines;
    uint32_t ui32BitsPerCycle;

    if (!pState)
    {
        return 0u;
    }

    ui32DataLines = xspi_nor_data_lines_get(pState->sHalCfg.sDevCfg.eIOMode);
    if (ui32DataLines == 0u)
    {
        return 0u;
    }

    ui32BitsPerCycle = ui32DataLines *
                       ((pState->sHalCfg.sDevCfg.eDataRate >=
                         AM_HAL_XSPIMC_DATA_RATE_S_S_D) ? 2u : 1u);
    if (ui32BitsPerCycle <= 8u)
    {
        return 1u;
    }

    for (uint32_t ui32AlignBytes = 1u; ui32AlignBytes <= 4u; ui32AlignBytes++)
    {
        if ((((uint64_t)ui32AlignBytes * 8ull) % (uint64_t)ui32BitsPerCycle) == 0ull)
        {
            return ui32AlignBytes;
        }
    }

    return 0u;
}

//*****************************************************************************
//
//! @brief Validate payload size against protocol transfer alignment.
//!
//! @param pState is the NOR runtime state.
//! @param ui32NumBytes is the requested payload size in bytes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_transfer_size_validate(const xspi_nor_state_t *pState, uint32_t ui32NumBytes)
{
    uint32_t ui32AlignBytes;

    if (ui32NumBytes == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    ui32AlignBytes = xspi_nor_transfer_align_bytes_get(pState);
    if (ui32AlignBytes == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((ui32NumBytes % ui32AlignBytes) != 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Round payload size up to the protocol-required alignment.
//!
//! @param pState is the NOR runtime state.
//! @param ui32NumBytes is the requested payload size in bytes.
//! @param pui32AlignedBytes points to returned aligned byte count.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_transfer_size_align_up(const xspi_nor_state_t *pState,
                                uint32_t ui32NumBytes,
                                uint32_t *pui32AlignedBytes)
{
    uint32_t ui32AlignBytes;
    uint64_t ui64AlignedBytes;

    if (!pui32AlignedBytes)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (ui32NumBytes == 0u)
    {
        *pui32AlignedBytes = 0u;
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    ui32AlignBytes = xspi_nor_transfer_align_bytes_get(pState);
    if (ui32AlignBytes == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui64AlignedBytes = ((((uint64_t)ui32NumBytes + (uint64_t)ui32AlignBytes - 1ull) /
                         (uint64_t)ui32AlignBytes) *
                        (uint64_t)ui32AlignBytes);
    if (ui64AlignedBytes > 0xFFFFFFFFull)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    *pui32AlignedBytes = (uint32_t)ui64AlignedBytes;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Check whether 1-byte payload must be expanded for 8D transfers.
//!
//! @param pState is the NOR runtime state.
//! @param ui32NumBytes is the requested payload size in bytes.
//!
//! @return True when logical 1-byte transfer needs 8D expansion.
//
//*****************************************************************************
static bool
xspi_nor_single_byte_expand_needed(const xspi_nor_state_t *pState,
                                   uint32_t ui32NumBytes)
{
    if (!pState || (ui32NumBytes != 1u))
    {
        return false;
    }

    return (pState->eCurrentProtocol == AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D);
}

//*****************************************************************************
//
//! @brief Resolve transfer mode based on DMA compatibility constraints.
//!
//! @param ePreferredMode is the caller preferred transfer mode.
//! @param pData points to payload buffer.
//! @param ui32NumBytes is the payload size in bytes.
//!
//! @return Effective transfer mode used for this packet.
//
//*****************************************************************************
static am_hal_xspimc_xfer_mode_t
xspi_nor_xfer_mode_resolve(am_hal_xspimc_xfer_mode_t ePreferredMode,
                           const uint8_t *pData,
                           uint32_t ui32NumBytes)
{
    if (ePreferredMode != AM_HAL_XSPIMC_DMA_XFER)
    {
        return ePreferredMode;
    }

    if ((ui32NumBytes == 0u) || !pData)
    {
        return AM_HAL_XSPIMC_PIO_XFER;
    }

    if (((ui32NumBytes & 0x3u) != 0u) || ((((uintptr_t)pData) & 0x3u) != 0u))
    {
        return AM_HAL_XSPIMC_PIO_XFER;
    }

    return AM_HAL_XSPIMC_DMA_XFER;
}

//*****************************************************************************
//
//! @brief Select next PIO chunk size while converging toward DMA alignment.
//!
//! @param pState is the NOR runtime state.
//! @param pData points to current transfer buffer position.
//! @param ui32NumBytes is number of bytes remaining.
//!
//! @return Next chunk size to process in PIO mode.
//
//*****************************************************************************
static uint32_t
xspi_nor_pio_step_get(const xspi_nor_state_t *pState,
                      const uint8_t *pData,
                      uint32_t ui32NumBytes)
{
    uint32_t ui32AlignBytes;
    uint32_t ui32Misalign;
    uint32_t ui32TargetBytes;

    if (!pState || !pData || (ui32NumBytes == 0u))
    {
        return 0u;
    }

    ui32AlignBytes = xspi_nor_transfer_align_bytes_get(pState);
    if ((ui32AlignBytes == 0u) || (ui32AlignBytes > 2u))
    {
        return 0u;
    }

    ui32Misalign    = ((uint32_t)(uintptr_t)pData) & 0x3u;
    ui32TargetBytes = ((ui32Misalign != 0u) && (ui32NumBytes >= 4u)) ?
                      (4u - ui32Misalign) : ui32NumBytes;

    if (ui32AlignBytes == 1u)
    {
        return ui32TargetBytes;
    }

    if ((ui32TargetBytes & 0x1u) == 0u)
    {
        return ui32TargetBytes;
    }

    if (ui32TargetBytes == 1u)
    {
        return 1u;
    }

    return (ui32Misalign & 0x1u) ? 1u : 2u;
}

//*****************************************************************************
//
// HAL Transfer Primitives
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Submit one custom command transfer to HAL.
//!
//! @param pState is the NOR runtime state.
//! @param ePreferredMode is preferred transfer mode for this packet.
//! @param pCfg points to transfer command/config attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_transfer_custom_mode(xspi_nor_state_t *pState,
                              am_hal_xspimc_xfer_mode_t ePreferredMode,
                              const xspi_nor_transfer_cfg_t *pCfg)
{
    am_hal_xspimc_xfer_packet_t sPacket = {0};
    am_hal_xspimc_xfer_t        sXfer   = {0};
    uint8_t                     *pXferData;
    xspi_nor_word_buf_t         sExpandBuf = {0};
    bool                        bExpandSingleByte;
    uint32_t                    ui32XferBytes;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pXferData         = pCfg->pData;
    ui32XferBytes     = pCfg->ui32NumBytes;
    bExpandSingleByte = xspi_nor_single_byte_expand_needed(pState,
                                                           pCfg->ui32NumBytes);
    if (bExpandSingleByte)
    {
        if (!pCfg->pData)
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        // Octal DDR transfers require a 16-bit physical payload. A single
        // logical byte is expanded into a 2-byte physical transfer.
        if (pCfg->eDir == AM_HAL_XSPIMC_XFER_WRITE)
        {
            sExpandBuf.aui8[0] = pCfg->pData[0];
            sExpandBuf.aui8[1] = pCfg->pData[0];
        }
        pXferData     = sExpandBuf.aui8;
        ui32XferBytes = 2u;
    }

    if (xspi_nor_transfer_size_validate(pState, ui32XferBytes))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sPacket.eDir         = pCfg->eDir;
    sPacket.ui16Cmd      = pCfg->ui16Cmd;
    sPacket.ui32Address  = pCfg->ui32Address;
    sPacket.ui32NumBytes = ui32XferBytes;
    sPacket.pDataBuff    = pXferData;

    sXfer.bAsync        = false;
    sXfer.eXferMode     = xspi_nor_xfer_mode_resolve(ePreferredMode, pXferData, ui32XferBytes);
    sXfer.ui8TxDummy    = pCfg->ui8TxDummy;
    sXfer.ui8RxDummy    = pCfg->ui8RxDummy;
    sXfer.ui32ModeBits  = pCfg->ui32ModeBits;
    sXfer.ui8ModeBitLen = pCfg->ui8ModeBitLen;
    sXfer.ui8CmdLen     = pCfg->ui8CmdLen;
    sXfer.ui8AddrLen    = pCfg->ui8AddrLen;
    sXfer.pPackets      = &sPacket;
    sXfer.ui32NumPacket = 1;
    sXfer.ui32Timeout   = AM_DEVICES_XSPI_NOR_TIMEOUT_US;

    if (am_hal_xspimc_xfer(pState->pBus, &pState->sDeviceID, &sXfer))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (bExpandSingleByte &&
        (pCfg->eDir == AM_HAL_XSPIMC_XFER_READ))
    {
        pCfg->pData[0] = sExpandBuf.aui8[0];
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Execute read transfer with aligned tail handling.
//!
//! @param pState is the NOR runtime state.
//! @param ePreferredMode is preferred transfer mode for this transaction.
//! @param pCfg points to transfer command/config attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_transfer_read_split_mode(xspi_nor_state_t *pState,
                                  am_hal_xspimc_xfer_mode_t ePreferredMode,
                                  const xspi_nor_transfer_cfg_t *pCfg)
{
    uint32_t            ui32AlignBytes;
    uint32_t            ui32DirectBytes;
    uint32_t            ui32TailBytes;
    xspi_nor_word_buf_t sTailBuf = {0};

    ui32AlignBytes = xspi_nor_transfer_align_bytes_get(pState);
    if (!pCfg || !pCfg->pData ||
        (ui32AlignBytes == 0u) ||
        (ui32AlignBytes > sizeof(sTailBuf.aui8)))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((ui32AlignBytes == 1u) ||
        ((pCfg->ui32NumBytes % ui32AlignBytes) == 0u))
    {
        return xspi_nor_transfer_custom_mode(pState,
                                             ePreferredMode,
                                             pCfg);
    }

    ui32TailBytes   = pCfg->ui32NumBytes % ui32AlignBytes;
    ui32DirectBytes = pCfg->ui32NumBytes - ui32TailBytes;
    if (ui32DirectBytes != 0u)
    {
        xspi_nor_transfer_cfg_t sDirectCfg = *pCfg;

        sDirectCfg.ui32NumBytes = ui32DirectBytes;
        if (xspi_nor_transfer_custom_mode(pState,
                                          ePreferredMode,
                                          &sDirectCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    xspi_nor_transfer_cfg_t sTailCfg = *pCfg;

    sTailCfg.ui32Address += ui32DirectBytes;
    sTailCfg.pData        = sTailBuf.aui8;
    sTailCfg.ui32NumBytes = ui32AlignBytes;
    if (xspi_nor_transfer_custom_mode(pState,
                                      ePreferredMode,
                                      &sTailCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    memcpy(&pCfg->pData[ui32DirectBytes], sTailBuf.aui8, ui32TailBytes);

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Execute data transfer using DMA/PIO mixed strategy when needed.
//!
//! @param pState is the NOR runtime state.
//! @param pCfg points to transfer command/config attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_data_transfer(xspi_nor_state_t *pState,
                       const xspi_nor_transfer_cfg_t *pCfg)
{
    uint8_t                *pData;
    uint32_t                ui32Remain;
    xspi_nor_transfer_cfg_t sXferCfg;
    uint32_t                ui32Status;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sXferCfg   = *pCfg;
    pData      = pCfg->pData;
    ui32Remain = pCfg->ui32NumBytes;

    if (pState->eXferMode != AM_HAL_XSPIMC_DMA_XFER)
    {
        if (sXferCfg.eDir == AM_HAL_XSPIMC_XFER_READ)
        {
            return xspi_nor_transfer_read_split_mode(pState,
                                                     pState->eXferMode,
                                                     &sXferCfg);
        }

        return xspi_nor_transfer_custom_mode(pState,
                                             pState->eXferMode,
                                             &sXferCfg);
    }

    while (ui32Remain != 0u)
    {
        uint32_t ui32Chunk;

        if ((((uintptr_t)pData & 0x3u) == 0u) && (ui32Remain >= 4u))
        {
            ui32Chunk = ui32Remain & ~0x3u;
            if (ui32Chunk != 0u)
            {
                sXferCfg.pData        = pData;
                sXferCfg.ui32NumBytes = ui32Chunk;
                if (sXferCfg.eDir == AM_HAL_XSPIMC_XFER_READ)
                {
                    ui32Status = xspi_nor_transfer_read_split_mode(
                        pState,
                        AM_HAL_XSPIMC_DMA_XFER,
                        &sXferCfg);
                }
                else
                {
                    ui32Status = xspi_nor_transfer_custom_mode(pState,
                                                               AM_HAL_XSPIMC_DMA_XFER,
                                                               &sXferCfg);
                }

                if (ui32Status)
                {
                    return ui32Status;
                }

                sXferCfg.ui32Address += ui32Chunk;
                pData                += ui32Chunk;
                ui32Remain           -= ui32Chunk;
                continue;
            }
        }

        ui32Chunk = xspi_nor_pio_step_get(pState, pData, ui32Remain);
        if (ui32Chunk == 0u)
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        sXferCfg.pData        = pData;
        sXferCfg.ui32NumBytes = ui32Chunk;
        if (sXferCfg.eDir == AM_HAL_XSPIMC_XFER_READ)
        {
            ui32Status = xspi_nor_transfer_read_split_mode(pState,
                                                           AM_HAL_XSPIMC_PIO_XFER,
                                                           &sXferCfg);
        }
        else
        {
            ui32Status = xspi_nor_transfer_custom_mode(pState,
                                                       AM_HAL_XSPIMC_PIO_XFER,
                                                       &sXferCfg);
        }

        if (ui32Status)
        {
            return ui32Status;
        }

        sXferCfg.ui32Address += ui32Chunk;
        pData                += ui32Chunk;
        ui32Remain           -= ui32Chunk;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Command Table Lookup Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Lookup command attributes in the currently active command set.
//!
//! @param pState is the NOR runtime state.
//! @param eCmd is the logical command identifier.
//!
//! @return Pointer to command attributes, or NULL if unsupported.
//
//*****************************************************************************
static const am_devices_xspi_nor_cmd_info_t *
xspi_nor_cmd_get(const xspi_nor_state_t *pState, am_devices_xspi_nor_cmd_e eCmd)
{
    if (!pState->pCmdSet || (eCmd >= AM_DEVICES_XSPI_NOR_CMD_MAX))
    {
        return NULL;
    }

    if (pState->pCmdSet->asCmdInfo[eCmd].ui16Cmd == 0u)
    {
        return NULL;
    }

    return &pState->pCmdSet->asCmdInfo[eCmd];
}

//*****************************************************************************
//
//! @brief Resolve active command-set table for a protocol.
//!
//! @param pDevice is the NOR device descriptor.
//! @param eProtocol is protocol to resolve.
//!
//! @return Pointer to command-set table, or NULL if unavailable.
//
//*****************************************************************************
static const am_devices_xspi_nor_cmd_set_t *
xspi_nor_cmd_set_resolve(const am_devices_xspi_nor_device_t *pDevice,
                         am_devices_xspi_nor_protocol_e eProtocol)
{
    if (pDevice->pCmdSets &&
        (pDevice->ui32NumCmdSets != 0u))
    {
        for (uint32_t i = 0; i < pDevice->ui32NumCmdSets; i++)
        {
            const am_devices_xspi_nor_cmd_set_t *pCmdSet = &pDevice->pCmdSets[i];

            if (pCmdSet->eProtocol == eProtocol)
            {
                return pCmdSet;
            }
        }
    }

    return am_devices_xspi_nor_cmd_set_get(eProtocol);
}

//*****************************************************************************
//
//! @brief Resolve active JEDEC command profile for a protocol.
//!
//! @param pDevice is the NOR device descriptor.
//! @param eProtocol is protocol to resolve.
//!
//! @return Pointer to JEDEC profile, or NULL if unavailable.
//
//*****************************************************************************
static const am_devices_xspi_nor_jedec_cmd_profile_t *
xspi_nor_jedec_profile_resolve(const am_devices_xspi_nor_device_t *pDevice,
                               am_devices_xspi_nor_protocol_e eProtocol)
{
    if (pDevice->pJEDECCmdProfiles &&
        (pDevice->ui32NumJEDECCmdProfiles != 0u))
    {
        for (uint32_t i = 0; i < pDevice->ui32NumJEDECCmdProfiles; i++)
        {
            const am_devices_xspi_nor_jedec_cmd_profile_t *pProfile =
                &pDevice->pJEDECCmdProfiles[i];

            if (pProfile->eProtocol == eProtocol)
            {
                return pProfile;
            }
        }
    }

    return am_devices_xspi_nor_jedec_cmd_profile_get(eProtocol);
}

//*****************************************************************************
//
//! @brief Check whether a protocol is supported by a device descriptor.
//!
//! @param pDevice is the NOR device descriptor.
//! @param eProtocol is protocol to validate.
//!
//! @return True when protocol is valid and supported.
//
//*****************************************************************************
static bool
xspi_nor_proto_supported(const am_devices_xspi_nor_device_t *pDevice,
                         am_devices_xspi_nor_protocol_e eProtocol)
{
    if ((eProtocol >= AM_DEVICES_XSPI_NOR_PROTO_MAX) ||
        (eProtocol == AM_DEVICES_XSPI_NOR_PROTO_INVALID))
    {
        return false;
    }

    if (pDevice->ui32SupportedProtoMask == 0u)
    {
        return true;
    }

    return ((pDevice->ui32SupportedProtoMask &
             AM_DEVICES_XSPI_NOR_PROTO_BIT(eProtocol)) != 0u);
}

//*****************************************************************************
//
//! @brief Resolve an RWW profile by exact target protocol.
//!
//! @param pDevice is the NOR device descriptor.
//! @param eProtocol is the target protocol to match.
//!
//! @return Pointer to the matching profile, or NULL if the wrapper declares no
//!         RWW support for this protocol.
//
//*****************************************************************************
static const am_devices_xspi_nor_rww_cfg_t *
xspi_nor_rww_profile_resolve(const am_devices_xspi_nor_device_t *pDevice,
                             am_devices_xspi_nor_protocol_e eProtocol)
{
    if (!pDevice || (pDevice->ui32NumRWWCfgs == 0u) || !pDevice->pRWWCfgs)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < pDevice->ui32NumRWWCfgs; i++)
    {
        if (pDevice->pRWWCfgs[i].eProtocol == eProtocol)
        {
            return &pDevice->pRWWCfgs[i];
        }
    }

    return NULL;
}

//*****************************************************************************
//
//! @brief Find an operation entry in an RWW profile's operation table.
//!
//! @param pProfile is the selected RWW profile.
//! @param eCmd is the logical command to locate.
//!
//! @return Pointer to the matching operation entry, or NULL if absent.
//
//*****************************************************************************
static const am_devices_xspi_nor_rww_op_t *
xspi_nor_rww_op_find(const am_devices_xspi_nor_rww_cfg_t *pProfile,
                     am_devices_xspi_nor_cmd_e eCmd)
{
    if (!pProfile || (pProfile->ui32NumOperations == 0u) || !pProfile->pOperations)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < pProfile->ui32NumOperations; i++)
    {
        if (pProfile->pOperations[i].eCmd == eCmd)
        {
            return &pProfile->pOperations[i];
        }
    }

    return NULL;
}

//*****************************************************************************
//
//! @brief Determine whether a logical command is a program/erase operation.
//!
//! @param eCmd is the logical command to test.
//!
//! @return true for page-program and erase commands only.
//
//*****************************************************************************
static bool
xspi_nor_rww_cmd_is_write_erase(am_devices_xspi_nor_cmd_e eCmd)
{
    switch (eCmd)
    {
        case AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM:
        case AM_DEVICES_XSPI_NOR_CMD_SECTOR_ERASE_4K:
        case AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_32K:
        case AM_DEVICES_XSPI_NOR_CMD_BLOCK_ERASE_64K:
        case AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE:
            return true;
        default:
            return false;
    }
}

static uint16_t xspi_nor_cmd_opcode_get(const xspi_nor_state_t *pState,
                                        const am_devices_xspi_nor_cmd_info_t *pCmd);

//*****************************************************************************
//
//! @brief Build the hardware-facing HAL suspend/resume configuration.
//!
//! @param pState   is the NOR runtime state with the active target command set.
//! @param pProfile is the selected wrapper RWW profile for the target protocol.
//! @param pCfg     is the output HAL configuration to populate.
//!
//! @return AM_DEVICES_XSPI_STATUS_SUCCESS on success; AM_DEVICES_XSPI_STATUS_ERROR
//!         on any validation failure (unsupported/fallback command, duplicate
//!         descriptor, array overflow, zero time, or unencodable length).
//
//*****************************************************************************
static uint32_t
xspi_nor_rww_hal_cfg_build(const xspi_nor_state_t *pState,
                           const am_devices_xspi_nor_rww_cfg_t *pProfile,
                           am_hal_xspimc_sus_res_cfg_t *pCfg)
{
    if (!pState || !pProfile || !pCfg || !pState->pCmdSet)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    // Suspend/resume commands must resolve directly in the active target set.
    const am_devices_xspi_nor_cmd_info_t *pSusCmd =
        xspi_nor_cmd_get(pState, pProfile->eSuspendCmd);
    const am_devices_xspi_nor_cmd_info_t *pResCmd =
        xspi_nor_cmd_get(pState, pProfile->eResumeCmd);
    if (!pSusCmd || !pResCmd)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    if ((pSusCmd->ui8CmdLen == 0u) || (pResCmd->ui8CmdLen == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    // The hardware has one suspend/resume command-length attribute.
    if (pSusCmd->ui8CmdLen != pResCmd->ui8CmdLen)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    memset(pCfg, 0, sizeof(*pCfg));
    pCfg->ui16SuspendCmd    = (uint16_t)xspi_nor_cmd_opcode_get(pState, pSusCmd);
    pCfg->ui16ResumeCmd     = (uint16_t)xspi_nor_cmd_opcode_get(pState, pResCmd);
    pCfg->ui8CmdLen         = pSusCmd->ui8CmdLen;
    pCfg->eIOMode           = pState->pCmdSet->eIOMode;
    pCfg->eDataRate         = pState->pCmdSet->eDataRate;
    pCfg->bAutoSusResEnable = true;

    pCfg->ui32SuspendTimeUs         = pProfile->ui32SuspendTimeUs;
    pCfg->ui32ResumeTimeUs          = pProfile->ui32ResumeTimeUs;
    pCfg->ui32SuspendToResumeTimeUs = pProfile->ui32SuspendToResumeTimeUs;
    pCfg->ui32ResumeToSuspendTimeUs = pProfile->ui32ResumeToSuspendTimeUs;

    // Build the bounded write/erase descriptor array from the profile's
    // operation table, resolving each command through the active target set.
    uint32_t ui32MaxBusyUs = 0u;
    for (uint32_t i = 0; i < pProfile->ui32NumOperations; i++)
    {
        const am_devices_xspi_nor_rww_op_t *pOp = &pProfile->pOperations[i];

        if (!xspi_nor_rww_cmd_is_write_erase(pOp->eCmd))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        if (pOp->ui32MaxBusyTimeUs == 0u)
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        const am_devices_xspi_nor_cmd_info_t *pCmd =
            xspi_nor_cmd_get(pState, pOp->eCmd);
        if (!pCmd || (pCmd->ui8CmdLen == 0u))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        uint16_t ui16OpCmd = (uint16_t)xspi_nor_cmd_opcode_get(pState, pCmd);

        // Reject duplicate descriptors.
        for (uint32_t j = 0; j < pCfg->ui8NumWriteEraseCmds; j++)
        {
            if (pCfg->asWriteEraseCmds[j].ui16Cmd == ui16OpCmd)
            {
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
        }
        if (pCfg->ui8NumWriteEraseCmds >= AM_HAL_XSPIMC_RWW_MAX_WRITE_ERASE_CMDS)
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        pCfg->asWriteEraseCmds[pCfg->ui8NumWriteEraseCmds].ui16Cmd   = ui16OpCmd;
        pCfg->asWriteEraseCmds[pCfg->ui8NumWriteEraseCmds].ui8CmdLen = pCmd->ui8CmdLen;
        pCfg->ui8NumWriteEraseCmds++;

        if (pOp->ui32MaxBusyTimeUs > ui32MaxBusyUs)
        {
            ui32MaxBusyUs = pOp->ui32MaxBusyTimeUs;
        }
    }

    // Holdoff = max busy time + guard time, with overflow checking.
    if (ui32MaxBusyUs > (0xFFFFFFFFu - pProfile->ui32HoldoffGuardTimeUs))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    pCfg->ui32HoldoffTimeUs = ui32MaxBusyUs + pProfile->ui32HoldoffGuardTimeUs;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Notify the HAL that a read-while-write operation completed early.
//!
//! @param pState is the NOR runtime state.
//!
//! Calls the HAL completion control only when RWW is enabled; otherwise a
//! no-op so callers can invoke it unconditionally after a confirmed WIP clear.
//
//*****************************************************************************
static void
xspi_nor_rww_complete_notify(const xspi_nor_state_t *pState)
{
    if (pState->bRWWEnabled)
    {
        uint32_t ui32Misc = AM_HAL_XSPIMC_MISC_RWW_HOLDOFF_CANCEL;
        (void)am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                    AM_HAL_XSPIMC_CONTROL_MISC_CONTROL,
                                    &ui32Misc);
    }
}

//*****************************************************************************
//
// Controller Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Check whether persistent controller configuration already matches.
//!
//! @param pState is the NOR runtime state.
//! @param eProtocol is the desired protocol.
//! @param ui32Freq is the desired controller frequency.
//! @param bDQS is the desired DQS setting.
//!
//! @return True when no persistent controller reconfiguration is required.
//
//*****************************************************************************
static bool
xspi_nor_controller_cfg_matches(const xspi_nor_state_t *pState,
                                am_devices_xspi_nor_protocol_e eProtocol,
                                uint32_t ui32Freq,
                                bool bDQS)
{
    return ((pState->eCurrentProtocol == eProtocol) &&
            (pState->sHalCfg.sDevCfg.ui32Freq == ui32Freq) &&
            (pState->sHalCfg.sDevCfg.bDQS == bDQS));
}

//*****************************************************************************
//
//! @brief Apply persistent controller configuration only when required.
//!
//! @param pFlash is the device handle.
//! @param eProtocol is the desired protocol.
//! @param ui32Freq is the desired controller frequency.
//! @param bDQS is the desired DQS setting.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_controller_config_apply_if_needed(am_device_xspi_periph_handle_t *pFlash,
                                           am_devices_xspi_nor_protocol_e eProtocol,
                                           uint32_t ui32Freq,
                                           bool bDQS)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    if (xspi_nor_controller_cfg_matches(pState, eProtocol, ui32Freq, bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    return am_devices_xspi_nor_apply_protocol(pFlash,
                                              eProtocol,
                                              ui32Freq,
                                              bDQS);
}

//*****************************************************************************
//
// Command Attribute Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Get active JEDEC command metadata for current protocol.
//!
//! @param pState is the NOR runtime state.
//!
//! @return Pointer to JEDEC command metadata, or NULL if not set.
//
//*****************************************************************************
static const am_devices_xspi_nor_jedec_cmd_info_t *
xspi_nor_jedec_cmd_info_get(const xspi_nor_state_t *pState)
{
    if (!pState->pJEDECCmdProfile)
    {
        return NULL;
    }

    return &pState->pJEDECCmdProfile->sCmdInfo;
}

//*****************************************************************************
//
//! @brief Select opcode variant based on current 4-byte mode policy.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to command transport attributes.
//! @return Opcode value to emit on the bus.
//
//*****************************************************************************
static uint16_t
xspi_nor_cmd_opcode_get(const xspi_nor_state_t *pState,
                        const am_devices_xspi_nor_cmd_info_t *pCmd)
{
    if (pState->bUse4ByteAddr && (pCmd->ui16Cmd4Byte != 0u))
    {
        return pCmd->ui16Cmd4Byte;
    }

    return pCmd->ui16Cmd;
}

//*****************************************************************************
//
//! @brief Get effective command address length for current address mode.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to command transport attributes.
//!
//! @return Address length in bytes for command execution.
//
//*****************************************************************************
static uint8_t
xspi_nor_cmd_addr_len_get(const xspi_nor_state_t *pState,
                          const am_devices_xspi_nor_cmd_info_t *pCmd)
{
    if ((pCmd->ui8AddrLen > 0u) && pState->bUse4ByteAddr)
    {
        return 4u;
    }

    return pCmd->ui8AddrLen;
}

//*****************************************************************************
//
//! @brief Compute effective read dummy cycles for data read command.
//!
//! @param pCmd points to command transport attributes.
//! @param pJEDEC points to active JEDEC metadata.
//!
//! @return Dummy-cycle count programmed for read transfer.
//
//*****************************************************************************
static uint8_t
xspi_nor_read_dummy_get(const am_devices_xspi_nor_cmd_info_t *pCmd,
                        const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC)
{
    if (pJEDEC)
    {
        return pJEDEC->ui8ReadDummyCycles;
    }

    return pCmd->ui8RxDummy;
}

//*****************************************************************************
//
// Transfer Config Builders
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Build transfer attributes from resolved command information.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved command attributes.
//! @param eDir is transfer direction for this command.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_cmd_cfg_build(const xspi_nor_state_t *pState,
                       const am_devices_xspi_nor_cmd_info_t *pCmd,
                       am_hal_xspimc_xfer_dir_e eDir,
                       xspi_nor_transfer_cfg_t *pOut)
{
    if (!pCmd || !pOut)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pOut->eDir          = eDir;
    pOut->ui16Cmd       = xspi_nor_cmd_opcode_get(pState, pCmd);
    pOut->ui8CmdLen     = pCmd->ui8CmdLen;
    pOut->ui8AddrLen    = xspi_nor_cmd_addr_len_get(pState, pCmd);
    pOut->ui8TxDummy    = pCmd->ui8TxDummy;
    pOut->ui8RxDummy    = pCmd->ui8RxDummy;
    pOut->ui8ModeBitLen = pCmd->ui8ModeBitLen;
    pOut->ui32ModeBits  = pCmd->ui32ModeBits;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Build transfer attributes for a register-read command.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_reg_read_cfg_build(const xspi_nor_state_t *pState,
                            const am_devices_xspi_nor_cmd_info_t *pCmd,
                            xspi_nor_transfer_cfg_t *pOut)
{
    const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC;

    if (xspi_nor_cmd_cfg_build(pState,
                               pCmd,
                               AM_HAL_XSPIMC_XFER_READ,
                               pOut))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pJEDEC = xspi_nor_jedec_cmd_info_get(pState);
    if (pJEDEC)
    {
        pOut->ui8RxDummy = pJEDEC->ui8RDSRDummy;
        pOut->ui8AddrLen = pJEDEC->bRDSRAddr4 ? 4u : 0u;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Build transfer attributes for a register-write or command transaction.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_reg_write_cfg_build(const xspi_nor_state_t *pState,
                             const am_devices_xspi_nor_cmd_info_t *pCmd,
                             xspi_nor_transfer_cfg_t *pOut)
{
    return xspi_nor_cmd_cfg_build(pState,
                                  pCmd,
                                  AM_HAL_XSPIMC_XFER_WRITE,
                                  pOut);
}

//*****************************************************************************
//
//! @brief Build transfer attributes for memory read traffic.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved READ command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_data_read_cfg_build(const xspi_nor_state_t *pState,
                             const am_devices_xspi_nor_cmd_info_t *pCmd,
                             xspi_nor_transfer_cfg_t *pOut)
{
    const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC;

    if (!pCmd || !pOut)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pJEDEC = xspi_nor_jedec_cmd_info_get(pState);

    pOut->eDir          = AM_HAL_XSPIMC_XFER_READ;
    pOut->ui16Cmd       = xspi_nor_cmd_opcode_get(pState, pCmd);
    pOut->ui8CmdLen     = pCmd->ui8CmdLen;
    pOut->ui8AddrLen    = xspi_nor_cmd_addr_len_get(pState, pCmd);
    pOut->ui8TxDummy    = pCmd->ui8TxDummy;
    pOut->ui8RxDummy    = xspi_nor_read_dummy_get(pCmd, pJEDEC);
    pOut->ui8ModeBitLen = pCmd->ui8ModeBitLen;
    pOut->ui32ModeBits  = pCmd->ui32ModeBits;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Build transfer attributes for page-program traffic.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved PAGE_PROGRAM command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_data_program_cfg_build(const xspi_nor_state_t *pState,
                                const am_devices_xspi_nor_cmd_info_t *pCmd,
                                xspi_nor_transfer_cfg_t *pOut)
{
    if (!pCmd || !pOut)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pOut->eDir          = AM_HAL_XSPIMC_XFER_WRITE;
    pOut->ui16Cmd       = xspi_nor_cmd_opcode_get(pState, pCmd);
    pOut->ui8CmdLen     = pCmd->ui8CmdLen;
    pOut->ui8AddrLen    = xspi_nor_cmd_addr_len_get(pState, pCmd);
    pOut->ui8TxDummy    = pCmd->ui8TxDummy;
    pOut->ui8RxDummy    = pCmd->ui8RxDummy;
    pOut->ui8ModeBitLen = pCmd->ui8ModeBitLen;
    pOut->ui32ModeBits  = pCmd->ui32ModeBits;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Build transfer attributes for JEDEC ID reads.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd optionally points to already-resolved command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_id_read_cfg_build(const xspi_nor_state_t *pState,
                           const am_devices_xspi_nor_cmd_info_t *pCmd,
                           xspi_nor_transfer_cfg_t *pOut)
{
    const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC;

    if (xspi_nor_cmd_cfg_build(pState,
                               pCmd,
                               AM_HAL_XSPIMC_XFER_READ,
                               pOut))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pJEDEC = xspi_nor_jedec_cmd_info_get(pState);
    if (pJEDEC)
    {
        pOut->ui8AddrLen = pJEDEC->bRDIDAddr4 ? 4u : 0u;
        pOut->ui8RxDummy = pJEDEC->ui8RDIDDummy;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Build transfer attributes for SFDP reads.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd optionally points to already-resolved command attributes.
//! @param pOut points to transfer attributes to populate.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_sfdp_read_cfg_build(const xspi_nor_state_t *pState,
                             const am_devices_xspi_nor_cmd_info_t *pCmd,
                             xspi_nor_transfer_cfg_t *pOut)
{
    const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC;

    if (xspi_nor_cmd_cfg_build(pState,
                               pCmd,
                               AM_HAL_XSPIMC_XFER_READ,
                               pOut))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pJEDEC = xspi_nor_jedec_cmd_info_get(pState);
    if (pJEDEC)
    {
        pOut->ui8AddrLen = pJEDEC->bSFDPAddr4 ? 4u : 3u;
        pOut->ui8RxDummy = pJEDEC->bSFDPDummy20 ? 20u : 8u;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Direct Current-Protocol Command Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Execute one write/command transaction without command-mode fallback.
//!
//! @param pFlash is the device handle.
//! @param pXferCfg is the logical write payload.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_reg_write_direct(am_device_xspi_periph_handle_t *pFlash,
                          xspi_nor_transfer_cfg_t        *pXferCfg)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    return xspi_nor_transfer_custom_mode(pState,
                                         pState->eXferMode,
                                         pXferCfg);
}

//*****************************************************************************
//
//! @brief Poll flash ready bit in the current protocol.
//!
//! @param pFlash is the device handle.
//! @param pCmd optionally points to already-resolved READ_STATUS attributes.
//! @param ui32TimeoutUs is max wait time in microseconds.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_wait_ready_direct(am_device_xspi_periph_handle_t *pFlash,
                           const am_devices_xspi_nor_cmd_info_t *pCmd,
                           uint32_t ui32TimeoutUs)
{
    xspi_nor_state_t       *pState = (xspi_nor_state_t *)pFlash->pData;
    uint32_t                ui32XferBytes;
    uint8_t                 ui8Status  = 0xFFu;
    xspi_nor_word_buf_t     sStatusBuf = {0};
    xspi_nor_transfer_cfg_t sXferCfg;
    uint32_t                ui32Status;

    if (xspi_nor_reg_read_cfg_build(pState,
                                    pCmd,
                                    &sXferCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = xspi_nor_transfer_size_align_up(pState, 1u, &ui32XferBytes);
    if ((ui32Status != AM_DEVICES_XSPI_STATUS_SUCCESS) ||
        (ui32XferBytes > sizeof(sStatusBuf.aui8)))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sXferCfg.ui32Address  = 0u;
    sXferCfg.pData        = sStatusBuf.aui8;
    sXferCfg.ui32NumBytes = ui32XferBytes;

    for (uint32_t i = 0; i < ui32TimeoutUs; i++)
    {
        memset(sStatusBuf.aui8, 0, sizeof(sStatusBuf.aui8));
        if (xspi_nor_transfer_custom_mode(pState,
                                          pState->eXferMode,
                                          &sXferCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        ui8Status = sStatusBuf.aui8[0];
        if ((ui8Status & AM_DEVICES_XSPI_NOR_SR_WIP) == 0u)
        {
            return AM_DEVICES_XSPI_STATUS_SUCCESS;
        }

        (void)am_hal_periph_delay(pFlash, 1u);
    }

    return AM_DEVICES_XSPI_STATUS_ERROR;
}

//*****************************************************************************
//
// Device Mode Switch Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Get mode-switch operation table for target protocol.
//!
//! @param pState is the NOR runtime state.
//! @param eProtocol is target protocol.
//!
//! @return Pointer to mode-switch descriptor, or NULL if none.
//
//*****************************************************************************
static const am_devices_xspi_nor_mode_switch_t *
xspi_nor_mode_switch_get(const xspi_nor_state_t *pState,
                         am_devices_xspi_nor_protocol_e eProtocol)
{
    if (!pState->pDevice->pModeSwitches)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < pState->pDevice->ui32NumModeSwitches; i++)
    {
        const am_devices_xspi_nor_mode_switch_t *pSwitch = &pState->pDevice->pModeSwitches[i];

        if (pSwitch->eProtocol == eProtocol)
        {
            return pSwitch;
        }
    }

    return NULL;
}

//*****************************************************************************
//
//! @brief Resolve default device mode domain from a protocol enum.
//!
//! @param eProtocol is the protocol to classify.
//!
//! @return Device mode domain used when no device override is provided.
//
//*****************************************************************************
static am_devices_xspi_nor_mode_domain_e
xspi_nor_mode_domain_default(am_devices_xspi_nor_protocol_e eProtocol)
{
    switch (eProtocol)
    {
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_SPI;

        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR;

        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_8D_8D:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_DTR;

        case AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_STR;

        case AM_DEVICES_XSPI_NOR_PROTO_2S_2D_2D:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_DTR;

        case AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_STR;

        case AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_DTR;

        case AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_STR;

        case AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_DTR;

        default:
            return AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DEVICE;
    }
}

//*****************************************************************************
//
//! @brief Resolve device mode domain for one protocol.
//!
//! @param pState is the NOR runtime state.
//! @param eProtocol is the protocol to classify.
//!
//! @return Effective device mode domain.
//
//*****************************************************************************
static am_devices_xspi_nor_mode_domain_e
xspi_nor_mode_domain_resolve(const xspi_nor_state_t *pState,
                             am_devices_xspi_nor_protocol_e eProtocol)
{
    const am_devices_xspi_nor_jedec_cmd_profile_t *pProfile;

    pProfile = xspi_nor_jedec_profile_resolve(pState->pDevice, eProtocol);
    if (pProfile &&
        (pProfile->sCmdInfo.eDeviceModeDomain != AM_DEVICES_XSPI_NOR_MODE_DOMAIN_AUTO))
    {
        return pProfile->sCmdInfo.eDeviceModeDomain;
    }

    return xspi_nor_mode_domain_default(eProtocol);
}

//*****************************************************************************
//
//! @brief Resolve JEDEC/device mode-enable requirement for one protocol.
//!
//! @param pState is the NOR runtime state.
//! @param eProtocol is the protocol to evaluate.
//!
//! @return Effective mode-enable requirement.
//
//*****************************************************************************
static am_devices_xspi_nor_mode_enable_e
xspi_nor_mode_enable_req_resolve(const xspi_nor_state_t *pState,
                                 am_devices_xspi_nor_protocol_e eProtocol)
{
    const am_devices_xspi_nor_mode_switch_t       *pSwitch;
    const am_devices_xspi_nor_jedec_cmd_profile_t *pProfile;

    pSwitch = xspi_nor_mode_switch_get(pState, eProtocol);
    if (pSwitch && (pSwitch->eModeEnableReq != AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE))
    {
        return pSwitch->eModeEnableReq;
    }

    pProfile = xspi_nor_jedec_profile_resolve(pState->pDevice, eProtocol);
    if (pProfile)
    {
        return pProfile->sCmdInfo.eModeEnableReq;
    }

    return AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE;
}

//*****************************************************************************
//
//! @brief Resolve control protocol for a protocol's effective device domain.
//!
//! @param pState is the NOR runtime state.
//! @param pProtocol points to returned protocol.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_control_protocol_resolve(const xspi_nor_state_t *pState,
                                  am_devices_xspi_nor_protocol_e *pProtocol)
{
    am_devices_xspi_nor_mode_domain_e eDomain;

    eDomain = xspi_nor_mode_domain_resolve(pState, pState->eCurrentProtocol);
    //dominate domain where register control is available based on the device.
    switch (eDomain)
    {
        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_SPI:
        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_STR:
        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_EXTSPI_DTR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_1S_1S_1S;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_STR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2S_2S;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_DPI_DTR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_2S_2D_2D;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_STR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4S_4S;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_QPI_DTR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_4S_4D_4D;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_STR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_8S_8S_8S;
            break;

        case AM_DEVICES_XSPI_NOR_MODE_DOMAIN_OPI_DTR:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_8D_8D_8D;
            break;

        default:
            *pProtocol = AM_DEVICES_XSPI_NOR_PROTO_INVALID;
            break;
    }

    if (!xspi_nor_proto_supported(pState->pDevice, *pProtocol))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Resolve bounded control frequency for mode-switch operations.
//!
//! @param pState is the NOR runtime state.
//!
//! @return Control frequency in Hz.
//
//*****************************************************************************
static uint32_t
xspi_nor_control_freq_get(const xspi_nor_state_t *pState)
{
    uint32_t ui32CurrentFreq = pState->sHalCfg.sDevCfg.ui32Freq;
    uint32_t ui32DefaultFreq = pState->pDevice->ui32DefaultFreq;
    uint32_t ui32ControlFreq = pState->pDevice->ui32ControlFreq;

    if (ui32ControlFreq == 0u)
    {
        return ui32CurrentFreq;
    }
    else
    {
        ui32ControlFreq = ui32ControlFreq > ui32DefaultFreq ? ui32DefaultFreq :
                                                              ui32ControlFreq;
    }

    if ((ui32ControlFreq == 0u) ||
        (ui32ControlFreq > AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ))
    {
        ui32ControlFreq = AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ;
    }

    return ui32ControlFreq;
}

//*****************************************************************************
//
//! @brief Execute one switch operation list in the current bus protocol.
//!
//! @param pFlash is the device handle.
//! @param pOps points to the operation list.
//! @param ui32NumOps is the number of operations.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_switch_ops_exec(am_device_xspi_periph_handle_t *pFlash,
                         const am_devices_xspi_nor_switch_op_t *pOps,
                         uint32_t ui32NumOps)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    if ((ui32NumOps != 0u) && (pOps == NULL))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    for (uint32_t i = 0; i < ui32NumOps; i++)
    {
        const am_devices_xspi_nor_switch_op_t *pOp = &pOps[i];
        xspi_nor_transfer_cfg_t               sXferCfg;

        if ((pOp->ui8CmdLen == 0u) || (pOp->ui16Cmd == 0u))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        sXferCfg.eDir          = AM_HAL_XSPIMC_XFER_WRITE;
        sXferCfg.ui16Cmd       = pOp->ui16Cmd;
        sXferCfg.ui8CmdLen     = pOp->ui8CmdLen;
        sXferCfg.ui8AddrLen    = pOp->ui8AddrLen;
        sXferCfg.ui8TxDummy    = pOp->ui8TxDummy;
        sXferCfg.ui8RxDummy    = 0u;
        sXferCfg.ui8ModeBitLen = 0u;
        sXferCfg.ui32ModeBits  = 0u;
        sXferCfg.ui32Address   = pOp->ui32Addr;
        sXferCfg.pData         = (uint8_t *)pOp->pui8Data;
        sXferCfg.ui32NumBytes  = pOp->ui32NumBytes;

        if (xspi_nor_transfer_custom_mode(pState,
                                          pState->eXferMode,
                                          &sXferCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Check whether a mode-enable requirement uses generic JEDEC handling.
//!
//! @param eReq is the mode-enable requirement.
//!
//! @return True when generic register-bit handling is available.
//
//*****************************************************************************
static bool
xspi_nor_mode_enable_req_is_generic(am_devices_xspi_nor_mode_enable_e eReq)
{
    switch (eReq)
    {
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_NONE:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V1:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S1B6:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B7:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V4:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V5:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_NONE:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_S2B3:
            return true;

        default:
            return false;
    }
}

//*****************************************************************************
//
//! @brief Read one byte using a JESD216F-defined register command.
//!
//! @param pFlash is the device handle.
//! @param ui16Cmd is the command opcode.
//! @param ui8AddrLen is command address length.
//! @param ui32Address is command address value.
//! @param ui8RxDummy is read dummy cycle count.
//! @param pui8Value points to returned register byte.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_jedec_reg_read(am_device_xspi_periph_handle_t *pFlash,
                        uint16_t ui16Cmd,
                        uint8_t ui8AddrLen,
                        uint32_t ui32Address,
                        uint8_t ui8RxDummy,
                        uint8_t *pui8Value)
{
    xspi_nor_state_t                           *pState = (xspi_nor_state_t *)pFlash->pData;
    const am_devices_xspi_nor_jedec_cmd_info_t *pJEDEC;
    xspi_nor_transfer_cfg_t                     sCfg = {0};

    if (!pui8Value)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pJEDEC = xspi_nor_jedec_cmd_info_get(pState);
    if (pJEDEC && (pJEDEC->ui8RDSRDummy != 0u))
    {
        ui8RxDummy = pJEDEC->ui8RDSRDummy;
    }

    sCfg.eDir         = AM_HAL_XSPIMC_XFER_READ;
    sCfg.ui16Cmd      = ui16Cmd;
    sCfg.ui8CmdLen    = 1u;
    sCfg.ui8AddrLen   = ui8AddrLen;
    sCfg.ui8RxDummy   = ui8RxDummy;
    sCfg.ui32Address  = ui32Address;
    sCfg.pData        = pui8Value;
    sCfg.ui32NumBytes = 1u;

    return xspi_nor_transfer_custom_mode(pState, pState->eXferMode, &sCfg);
}

//*****************************************************************************
//
//! @brief Write one byte using a JESD216F-defined register command.
//!
//! @param pFlash is the device handle.
//! @param ui16Cmd is the command opcode.
//! @param ui8Value is the value to write.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_jedec_reg_write(am_device_xspi_periph_handle_t *pFlash,
                         uint16_t ui16Cmd,
                         uint8_t ui8Value)
{
    xspi_nor_state_t        *pState = (xspi_nor_state_t *)pFlash->pData;
    xspi_nor_transfer_cfg_t  sCfg   = {0};

    sCfg.eDir      = AM_HAL_XSPIMC_XFER_WRITE;
    sCfg.ui16Cmd   = AM_DEVICES_XSPI_NOR_OP_WREN;
    sCfg.ui8CmdLen = 1u;

    if (xspi_nor_reg_write_direct(pFlash, &sCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sCfg.eDir         = AM_HAL_XSPIMC_XFER_WRITE;
    sCfg.ui16Cmd      = ui16Cmd;
    sCfg.ui8CmdLen    = 1u;
    sCfg.pData        = &ui8Value;
    sCfg.ui32NumBytes = 1u;

    if (xspi_nor_transfer_custom_mode(pState, pState->eXferMode, &sCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Write status register 1/2 through JESD216F two-byte WRSR.
//!
//! @param pFlash is the device handle.
//! @param ui8Status1 is status register byte 1.
//! @param ui8Status2 is status register byte 2.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_status_2byte_write(am_device_xspi_periph_handle_t *pFlash,
                            uint8_t ui8Status1,
                            uint8_t ui8Status2)
{
    xspi_nor_state_t        *pState = (xspi_nor_state_t *)pFlash->pData;
    xspi_nor_transfer_cfg_t  sCfg   = {0};
    uint8_t                  aui8Status[2];

    aui8Status[0] = ui8Status1;
    aui8Status[1] = ui8Status2;

    sCfg.eDir      = AM_HAL_XSPIMC_XFER_WRITE;
    sCfg.ui16Cmd   = AM_DEVICES_XSPI_NOR_OP_WREN;
    sCfg.ui8CmdLen = 1u;

    if (xspi_nor_reg_write_direct(pFlash, &sCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sCfg.eDir         = AM_HAL_XSPIMC_XFER_WRITE;
    sCfg.ui16Cmd      = AM_DEVICES_XSPI_NOR_OP_WRSR;
    sCfg.ui8CmdLen    = 1u;
    sCfg.pData        = aui8Status;
    sCfg.ui32NumBytes = sizeof(aui8Status);

    if (xspi_nor_transfer_custom_mode(pState,
                                      pState->eXferMode,
                                      &sCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Set or clear one bit through JESD216F register commands.
//!
//! @param pFlash is the device handle.
//! @param ui16ReadCmd is the register read opcode.
//! @param ui8ReadAddrLen is read address length.
//! @param ui32ReadAddress is read address value.
//! @param ui8ReadDummy is read dummy cycle count.
//! @param ui16WriteCmd is the register write opcode.
//! @param ui8BitMask is the bit mask to update.
//! @param bEnable selects set or clear operation.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_jedec_reg_bit_update(am_device_xspi_periph_handle_t *pFlash,
                              uint16_t ui16ReadCmd,
                              uint8_t ui8ReadAddrLen,
                              uint32_t ui32ReadAddress,
                              uint8_t ui8ReadDummy,
                              uint16_t ui16WriteCmd,
                              uint8_t ui8BitMask,
                              bool bEnable)
{
    uint8_t ui8Value;

    if (xspi_nor_jedec_reg_read(pFlash,
                                ui16ReadCmd,
                                ui8ReadAddrLen,
                                ui32ReadAddress,
                                ui8ReadDummy,
                                &ui8Value))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (bEnable)
    {
        ui8Value |= ui8BitMask;
    }
    else
    {
        ui8Value &= (uint8_t) ~ui8BitMask;
    }

    return xspi_nor_jedec_reg_write(pFlash, ui16WriteCmd, ui8Value);
}

//*****************************************************************************
//
//! @brief Set or clear one bit in status register 2 using two-byte WRSR.
//!
//! @param pFlash is the device handle.
//! @param ui8BitMask is the status register 2 bit mask.
//! @param bEnable selects set or clear operation.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_status2_bit_update_2byte(am_device_xspi_periph_handle_t *pFlash,
                                  uint8_t ui8BitMask,
                                  bool bEnable)
{
    uint8_t ui8Status1;
    uint8_t ui8Status2;

    if (xspi_nor_jedec_reg_read(pFlash,
                                AM_DEVICES_XSPI_NOR_OP_RDSR,
                                0u,
                                0u,
                                0u,
                                &ui8Status1) ||
        xspi_nor_jedec_reg_read(pFlash,
                                AM_DEVICES_XSPI_NOR_OP_RDSR2,
                                0u,
                                0u,
                                0u,
                                &ui8Status2))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (bEnable)
    {
        ui8Status2 |= ui8BitMask;
    }
    else
    {
        ui8Status2 &= (uint8_t) ~ui8BitMask;
    }

    return xspi_nor_status_2byte_write(pFlash, ui8Status1, ui8Status2);
}

//*****************************************************************************
//
//! @brief Set or clear the JESD216F Octal Enable bit.
//!
//! @param pFlash is the device handle.
//! @param bEnable selects set or clear operation.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_oer_s2b3_update(am_device_xspi_periph_handle_t *pFlash,
                         bool bEnable)
{
    uint8_t  ui8Status;
    uint16_t ui16WriteCmd;

    if (xspi_nor_jedec_reg_read(pFlash,
                                AM_DEVICES_XSPI_NOR_OP_RDOER,
                                1u,
                                0x02u,
                                8u,
                                &ui8Status))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (bEnable)
    {
        ui8Status |= 0x08u;
    }
    else
    {
        ui8Status &= (uint8_t) ~0x08u;
    }

    ui16WriteCmd = bEnable ? AM_DEVICES_XSPI_NOR_OP_WRSR2 :
                             AM_DEVICES_XSPI_NOR_OP_WRSR2_ALT;

    return xspi_nor_jedec_reg_write(pFlash, ui16WriteCmd, ui8Status);
}

//*****************************************************************************
//
//! @brief Execute generic JEDEC QER/OER mode-enable procedure.
//!
//! @param pFlash is the device handle.
//! @param eReq is the concrete JEDEC enable requirement.
//! @param bEnable selects enable or disable operation.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mode_enable_req_apply(am_device_xspi_periph_handle_t *pFlash,
                               am_devices_xspi_nor_mode_enable_e eReq,
                               bool bEnable)
{
    switch (eReq)
    {
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_NONE:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_NONE:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_NONE:
            return AM_DEVICES_XSPI_STATUS_SUCCESS;

        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S1B6:
            return xspi_nor_jedec_reg_bit_update(pFlash,
                                                 AM_DEVICES_XSPI_NOR_OP_RDSR,
                                                 0u,
                                                 0u,
                                                 0u,
                                                 AM_DEVICES_XSPI_NOR_OP_WRSR,
                                                 0x40u,
                                                 bEnable);

        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V1:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V4:
        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V5:
            return xspi_nor_status2_bit_update_2byte(pFlash, 0x02u, bEnable);

        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B7:
            return xspi_nor_jedec_reg_bit_update(pFlash,
                                                 AM_DEVICES_XSPI_NOR_OP_RDSR2_ALT,
                                                 0u,
                                                 0u,
                                                 0u,
                                                 AM_DEVICES_XSPI_NOR_OP_WRSR2_ALT,
                                                 0x80u,
                                                 bEnable);

        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_QER_S2B1V6:
            return xspi_nor_jedec_reg_bit_update(pFlash,
                                                 AM_DEVICES_XSPI_NOR_OP_RDSR2,
                                                 0u,
                                                 0u,
                                                 0u,
                                                 AM_DEVICES_XSPI_NOR_OP_WRSR2,
                                                 0x02u,
                                                 bEnable);

        case AM_DEVICES_XSPI_NOR_MODE_ENABLE_OER_S2B3:
            return xspi_nor_oer_s2b3_update(pFlash, bEnable);

        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }
}

//*****************************************************************************
//
//! @brief Execute device-specific mode switch operation sequence.
//!
//! @param pFlash is the device handle.
//! @param eTargetProtocol is protocol to switch into.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mode_switch(am_device_xspi_periph_handle_t *pFlash,
                     am_devices_xspi_nor_protocol_e eTargetProtocol)
{
    xspi_nor_state_t                        *pState = (xspi_nor_state_t *)pFlash->pData;
    const am_devices_xspi_nor_mode_switch_t *pFromSwitch;
    const am_devices_xspi_nor_mode_switch_t *pToSwitch;
    am_devices_xspi_nor_mode_domain_e        eFromDomain;
    am_devices_xspi_nor_mode_domain_e        eToDomain;
    am_devices_xspi_nor_mode_enable_e        eFromReq;
    am_devices_xspi_nor_mode_enable_e        eToReq;
    bool                                     bExitNeeded;
    bool                                     bEnterNeeded;
    bool                                     bExitReqNeeded;
    bool                                     bEnterReqNeeded;
    uint32_t                                 ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if ((eTargetProtocol == pState->eCurrentProtocol) ||
        (pState->eCurrentProtocol == AM_DEVICES_XSPI_NOR_PROTO_INVALID))
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    pFromSwitch = xspi_nor_mode_switch_get(pState, pState->eCurrentProtocol);
    pToSwitch   = xspi_nor_mode_switch_get(pState, eTargetProtocol);
    eFromDomain = xspi_nor_mode_domain_resolve(pState, pState->eCurrentProtocol);
    eToDomain   = xspi_nor_mode_domain_resolve(pState, eTargetProtocol);
    eFromReq    = xspi_nor_mode_enable_req_resolve(pState, pState->eCurrentProtocol);
    eToReq      = xspi_nor_mode_enable_req_resolve(pState, eTargetProtocol);

    if ((eFromDomain == eToDomain) &&
        (eFromReq == eToReq))
    {
        //Extended SPI returns
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    bExitReqNeeded = (pFromSwitch &&
                      pFromSwitch->bDisableOnExit &&
                      (eFromReq != eToReq) &&
                      xspi_nor_mode_enable_req_is_generic(eFromReq));
    bEnterReqNeeded = ((eToReq != eFromReq) &&
                       xspi_nor_mode_enable_req_is_generic(eToReq));
    bExitNeeded     = (pFromSwitch && (pFromSwitch->ui32NumExitOps != 0u));
    bEnterNeeded    = (pToSwitch && (pToSwitch->ui32NumEnterOps != 0u));

    if (!bExitReqNeeded && !bExitNeeded &&
        !bEnterReqNeeded && !bEnterNeeded)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    pState->bBypassControlSwitch = true;

    if (bExitNeeded)
    {
        ui32Status = xspi_nor_switch_ops_exec(pFlash,
                                              pFromSwitch->pExitOps,
                                              pFromSwitch->ui32NumExitOps);
        if (ui32Status)
        {
            goto mode_switch_cleanup;
        }
    }

    if (bExitReqNeeded)
    {
        ui32Status = xspi_nor_mode_enable_req_apply(pFlash, eFromReq, false);
        if (ui32Status)
        {
            goto mode_switch_cleanup;
        }
    }

    if ((bExitReqNeeded || bExitNeeded) &&
        (bEnterReqNeeded || bEnterNeeded))
    {
        ui32Status = xspi_nor_controller_config_apply_if_needed(pFlash,
                                                                pState->pDevice->eDefaultProtocol,
                                                                xspi_nor_control_freq_get(pState),
                                                                pState->sHalCfg.sDevCfg.bDQS);
        if (ui32Status)
        {
            goto mode_switch_cleanup;
        }
    }

    if (bEnterReqNeeded)
    {
        ui32Status = xspi_nor_mode_enable_req_apply(pFlash, eToReq, true);
        if (ui32Status)
        {
            goto mode_switch_cleanup;
        }
    }

    if (bEnterNeeded)
    {
        ui32Status = xspi_nor_switch_ops_exec(pFlash,
                                              pToSwitch->pEnterOps,
                                              pToSwitch->ui32NumEnterOps);
    }

mode_switch_cleanup:
    pState->bBypassControlSwitch = false;
    return ui32Status;
}

//*****************************************************************************
//
// Command-Mode Policy Wrappers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Check whether a protocol is an extended-SPI protocol.
//!
//! @param eProtocol is the protocol to evaluate.
//!
//! @return True when command phase uses one line and later phases widen.
//!
//
//*****************************************************************************
static bool
xspi_nor_protocol_is_extended(am_devices_xspi_nor_protocol_e eProtocol)
{
    switch (eProtocol)
    {
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_2S_2S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_2D_2D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_4S_4S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_4D_4D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8D:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_8S_8S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_8D_8D:
            return true;

        default:
            return false;
    }
}

//*****************************************************************************
//
//! @brief Check whether an extended protocol keeps address phase on one line.
//!
//! @param eProtocol is the protocol to evaluate.
//!
//! @return True for a 1S-1S-xS protocol.
//!
//
//*****************************************************************************
static bool
xspi_nor_protocol_has_serial_address(am_devices_xspi_nor_protocol_e eProtocol)
{
    switch (eProtocol)
    {
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_2S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_4S:
        case AM_DEVICES_XSPI_NOR_PROTO_1S_1S_8S:
            return true;

        default:
            return false;
    }
}

//*****************************************************************************
//
//! @brief Check whether control command geometry is direct in extended SPI.
//!
//! @param eProtocol is the active extended-SPI protocol.
//! @param eDir is the command transfer direction.
//! @param ui32NumBytes is the command payload size.
//! @param pCmd points to command attributes from the control table.
//!
//! @return True for compatible 1-0-0 or 1-1-0 command geometry.
//!
//
//*****************************************************************************
static bool
xspi_nor_extended_control_cmd_is_direct(am_devices_xspi_nor_protocol_e eProtocol,
                                        am_hal_xspimc_xfer_dir_e eDir,
                                        uint32_t ui32NumBytes,
                                        const am_devices_xspi_nor_cmd_info_t *pCmd)
{
    if ((eDir != AM_HAL_XSPIMC_XFER_WRITE) ||
        (ui32NumBytes != 0u))
    {
        return false;
    }

    if (pCmd->ui8AddrLen == 0u)
    {
        return true;
    }

    return xspi_nor_protocol_has_serial_address(eProtocol);
}

//*****************************************************************************
//
//! @brief Return command policy metadata for a logical command.
//!
//! @param eCmd is the logical command identifier.
//!
//! @return Pointer to command policy entry, or NULL if invalid.
//
//*****************************************************************************
static const am_devices_xspi_nor_cmd_policy_t *
xspi_nor_cmd_policy_get(am_devices_xspi_nor_cmd_e eCmd)
{
    if (eCmd >= AM_DEVICES_XSPI_NOR_CMD_MAX)
    {
        return NULL;
    }

    return &g_sCmdPolicies[eCmd];
}

//*****************************************************************************
//
//! @brief Resolve a command through ordered device-specific fallback policy.
//!
//! @param pState is the NOR runtime state.
//! @param eCmd is the logical command identifier.
//! @param pProtocol points to resolved fallback protocol.
//! @param ppCmd points to resolved command attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_device_cmd_fallback_resolve(const xspi_nor_state_t *pState,
                                     am_devices_xspi_nor_cmd_e eCmd,
                                     am_devices_xspi_nor_protocol_e *pProtocol,
                                     const am_devices_xspi_nor_cmd_info_t **ppCmd)
{
    const am_devices_xspi_nor_device_t *pDevice;
    uint32_t                            ui32CmdBit;

    pDevice    = pState->pDevice;
    ui32CmdBit = AM_DEVICES_XSPI_NOR_CMD_BIT(eCmd);

    if (!pDevice->pCmdFallbacks)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    for (uint32_t i = 0u; i < pDevice->ui32NumCmdFallbacks; i++)
    {
        const am_devices_xspi_nor_cmd_fallback_t *pFallback =
            &pDevice->pCmdFallbacks[i];
        const am_devices_xspi_nor_cmd_set_t *pSet;

        if ((pFallback->eSourceProtocol != pState->eCurrentProtocol) ||
            ((pFallback->ui32CmdMask & ui32CmdBit) == 0u) ||
            !xspi_nor_proto_supported(pDevice, pFallback->eFallbackProtocol))
        {
            continue;
        }

        pSet = xspi_nor_cmd_set_resolve(pDevice, pFallback->eFallbackProtocol);
        if (!pSet || (pSet->asCmdInfo[eCmd].ui16Cmd == 0u))
        {
            continue;
        }

        *pProtocol = pFallback->eFallbackProtocol;
        *ppCmd     = &pSet->asCmdInfo[eCmd];
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    return AM_DEVICES_XSPI_STATUS_ERROR;
}

//*****************************************************************************
//
//! @brief Resolve protocol to execute a register-class command.
//!
//! @param pState is the NOR runtime state.
//! @param eCmd is the logical command identifier.
//! @param eDir is transfer direction for the command.
//! @param ui32NumBytes is payload size in bytes.
//! @param pProtocol points to resolved execution protocol.
//! @param ppCmd points to resolved command attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_command_protocol_resolve(const xspi_nor_state_t *pState,
                                  am_devices_xspi_nor_cmd_e eCmd,
                                  am_hal_xspimc_xfer_dir_e eDir,
                                  uint32_t ui32NumBytes,
                                  am_devices_xspi_nor_protocol_e *pProtocol,
                                  const am_devices_xspi_nor_cmd_info_t **ppCmd)
{
    const am_devices_xspi_nor_cmd_policy_t *pPolicy;
    const am_devices_xspi_nor_cmd_set_t    *pFallbackSet;
    const am_devices_xspi_nor_cmd_info_t   *pCmd;
    am_devices_xspi_nor_protocol_e          eFallbackProtocol;

    pPolicy = xspi_nor_cmd_policy_get(eCmd);
    if (!pPolicy || !pPolicy->bRegisterClass)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pCmd = xspi_nor_cmd_get(pState, eCmd);
    if (pCmd)
    {
        *pProtocol = pState->eCurrentProtocol;
        *ppCmd     = pCmd;
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    if (xspi_nor_device_cmd_fallback_resolve(pState,
                                             eCmd,
                                             &eFallbackProtocol,
                                             &pCmd))
    {
        //generic fallback
        if (xspi_nor_control_protocol_resolve(pState, &eFallbackProtocol))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        pFallbackSet = xspi_nor_cmd_set_resolve(pState->pDevice,
                                                eFallbackProtocol);
        if (!pFallbackSet ||
            (pFallbackSet->asCmdInfo[eCmd].ui16Cmd == 0u))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        pCmd = &pFallbackSet->asCmdInfo[eCmd];
    }

    if (xspi_nor_protocol_is_extended(pState->eCurrentProtocol) &&
        xspi_nor_extended_control_cmd_is_direct(pState->eCurrentProtocol,
                                                eDir,
                                                ui32NumBytes,
                                                pCmd))
    {
        *pProtocol = pState->eCurrentProtocol;
    }
    else
    {
        *pProtocol = eFallbackProtocol;
    }

    *ppCmd = pCmd;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Resolve protocol to execute a memory-class command.
//!
//! @param pState is the NOR runtime state.
//! @param eCmd is the logical command identifier.
//! @param pProtocol points to resolved execution protocol.
//! @param ppCmd points to resolved command attributes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mem_protocol_resolve(const xspi_nor_state_t *pState,
                              am_devices_xspi_nor_cmd_e eCmd,
                              am_devices_xspi_nor_protocol_e *pProtocol,
                              const am_devices_xspi_nor_cmd_info_t **ppCmd)
{
    const am_devices_xspi_nor_cmd_policy_t *pPolicy;
    const am_devices_xspi_nor_cmd_set_t    *pFallbackSet;
    const am_devices_xspi_nor_cmd_info_t   *pCmd;
    am_devices_xspi_nor_protocol_e          eFallbackProtocol;

    pPolicy = xspi_nor_cmd_policy_get(eCmd);
    if (!pPolicy || pPolicy->bRegisterClass)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pCmd = xspi_nor_cmd_get(pState, eCmd);
    if (pCmd)
    {
        *pProtocol = pState->eCurrentProtocol;
        *ppCmd     = pCmd;
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    if (xspi_nor_device_cmd_fallback_resolve(pState,
                                             eCmd,
                                             &eFallbackProtocol,
                                             &pCmd))
    {
        if (xspi_nor_control_protocol_resolve(pState, &eFallbackProtocol))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        pFallbackSet = xspi_nor_cmd_set_resolve(pState->pDevice,
                                                eFallbackProtocol);
        if (!pFallbackSet ||
            (pFallbackSet->asCmdInfo[eCmd].ui16Cmd == 0u))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        pCmd = &pFallbackSet->asCmdInfo[eCmd];
    }

    *pProtocol = eFallbackProtocol;
    *ppCmd     = pCmd;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Prepare control-mode protocol/frequency for register command.
//!
//! @param pFlash is the device handle.
//! @param eCmd is the logical command identifier.
//! @param eDir is transfer direction for the command.
//! @param ui32NumBytes is payload size in bytes.
//! @param pCtx points to saved context used for restore path.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_command_mode_prepare(am_device_xspi_periph_handle_t *pFlash,
                              am_devices_xspi_nor_cmd_e eCmd,
                              am_hal_xspimc_xfer_dir_e eDir,
                              uint32_t ui32NumBytes,
                              xspi_nor_mode_ctx_t *pCtx)
{
    xspi_nor_state_t               *pState;
    am_devices_xspi_nor_protocol_e  eControlProtocol;
    uint32_t                        ui32ControlFreq;

    if (!pCtx)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    pCtx->bSwitched = false;
    pCtx->eProtocol = pState->eCurrentProtocol;
    pCtx->ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    pCtx->bDQS      = pState->sHalCfg.sDevCfg.bDQS;
    pCtx->pCmd      = NULL;

    if (pState->bBypassControlSwitch)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    if (xspi_nor_command_protocol_resolve(pState,
                                          eCmd,
                                          eDir,
                                          ui32NumBytes,
                                          &eControlProtocol,
                                          &pCtx->pCmd))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32ControlFreq = xspi_nor_control_freq_get(pState);
    if (xspi_nor_controller_cfg_matches(pState,
                                        eControlProtocol,
                                        ui32ControlFreq,
                                        pCtx->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    if (xspi_nor_mode_switch(pFlash, eControlProtocol))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           eControlProtocol,
                                           ui32ControlFreq,
                                           pCtx->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pCtx->bSwitched = true;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Restore protocol/frequency after control-mode command execution.
//!
//! @param pFlash is the device handle.
//! @param pCtx points to saved mode context from prepare path.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_command_mode_restore(am_device_xspi_periph_handle_t *pFlash,
                              const xspi_nor_mode_ctx_t *pCtx)
{
    if (!pCtx)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (!pCtx->bSwitched)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    if (xspi_nor_mode_switch(pFlash, pCtx->eProtocol))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_controller_config_apply_if_needed(pFlash,
                                                   pCtx->eProtocol,
                                                   pCtx->ui32Freq,
                                                   pCtx->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Prepare a supported protocol for a memory command.
//!
//! @param pFlash is the device handle.
//! @param eCmd is READ or PAGE_PROGRAM.
//! @param pCtx points to saved context used for restore path.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mem_mode_prepare(am_device_xspi_periph_handle_t *pFlash,
                          am_devices_xspi_nor_cmd_e eCmd,
                          xspi_nor_mode_ctx_t *pCtx)
{
    xspi_nor_state_t              *pState;
    am_devices_xspi_nor_protocol_e eProtocol;
    uint32_t                       ui32Freq;

    if (!pCtx)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    pCtx->bSwitched = false;
    pCtx->eProtocol = pState->eCurrentProtocol;
    pCtx->ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    pCtx->bDQS      = pState->sHalCfg.sDevCfg.bDQS;
    pCtx->pCmd      = NULL;

    if (xspi_nor_mem_protocol_resolve(pState,
                                      eCmd,
                                      &eProtocol,
                                      &pCtx->pCmd))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (eProtocol == pState->eCurrentProtocol)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    pCtx->bSwitched = true;

    if (xspi_nor_mode_switch(pFlash, eProtocol))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_controller_config_apply_if_needed(pFlash,
                                                   eProtocol,
                                                   pCtx->ui32Freq,
                                                   pCtx->bDQS))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Restore protocol configuration after a memory command.
//!
//! @param pFlash is the device handle.
//! @param pCtx points to saved mode context from prepare path.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mem_mode_restore(am_device_xspi_periph_handle_t *pFlash,
                          const xspi_nor_mode_ctx_t *pCtx)
{
    return xspi_nor_command_mode_restore(pFlash, pCtx);
}

//*****************************************************************************
//
//! @brief Poll flash ready bit until timeout expires.
//!
//! @param pFlash is the device handle.
//! @param ui32TimeoutUs is max wait time in microseconds.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_wait_ready(am_device_xspi_periph_handle_t *pFlash, uint32_t ui32TimeoutUs)
{
    xspi_nor_mode_ctx_t sModeCtx = {0};
    uint32_t            ui32Status;

    if (xspi_nor_command_mode_prepare(pFlash,
                                      AM_DEVICES_XSPI_NOR_CMD_READ_STATUS,
                                      AM_HAL_XSPIMC_XFER_READ,
                                      1u,
                                      &sModeCtx))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = xspi_nor_wait_ready_direct(pFlash, sModeCtx.pCmd, ui32TimeoutUs);

    if (xspi_nor_command_mode_restore(pFlash, &sModeCtx))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return ui32Status;
}

//*****************************************************************************
//
//! @brief Execute a register-write, no-payload, or address-command transaction.
//!
//! @param pFlash is the device handle.
//! @param eCmd is the logical write/command identifier.
//! @param ui32Address is command address argument.
//! @param pData points to optional write payload.
//! @param ui32NumBytes is payload size in bytes.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_reg_write_exec(am_device_xspi_periph_handle_t *pFlash,
                        am_devices_xspi_nor_cmd_e eCmd,
                        uint32_t ui32Address,
                        uint8_t *pData,
                        uint32_t ui32NumBytes)
{
    xspi_nor_state_t       *pState   = (xspi_nor_state_t *)pFlash->pData;
    xspi_nor_mode_ctx_t     sModeCtx = {0};
    xspi_nor_transfer_cfg_t sXferCfg;
    uint32_t                ui32Status;
    uint32_t                ui32RestoreStatus;

    if (xspi_nor_command_mode_prepare(pFlash,
                                      eCmd,
                                      AM_HAL_XSPIMC_XFER_WRITE,
                                      ui32NumBytes,
                                      &sModeCtx))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = xspi_nor_reg_write_cfg_build(pState,
                                              sModeCtx.pCmd,
                                              &sXferCfg);
    if (ui32Status == AM_DEVICES_XSPI_STATUS_SUCCESS)
    {
        sXferCfg.ui32Address  = ui32Address;
        sXferCfg.pData        = pData;
        sXferCfg.ui32NumBytes = ui32NumBytes;
        ui32Status            = xspi_nor_transfer_custom_mode(pState,
                                                              pState->eXferMode,
                                                              &sXferCfg);
    }

    ui32RestoreStatus = xspi_nor_command_mode_restore(pFlash, &sModeCtx);
    if (ui32Status)
    {
        return ui32Status;
    }
    if (ui32RestoreStatus)
    {
        return ui32RestoreStatus;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Issue write-enable command.
//!
//! @param pFlash is the device handle.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_write_enable(am_device_xspi_periph_handle_t *pFlash)
{
    return xspi_nor_reg_write_exec(pFlash,
                                   AM_DEVICES_XSPI_NOR_CMD_WRITE_ENABLE,
                                   0u,
                                   NULL,
                                   0u);
}

//*****************************************************************************
//
//! @brief Run reset sequence for the active NOR device.
//!
//! @param pFlash is the device handle.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_reset(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    const am_devices_xspi_nor_quirks_t *pQuirks = pState->pDevice->pQuirks;
    am_devices_xspi_nor_protocol_cfg_t sProtocolCfg;

    sProtocolCfg.eProtocol = pState->eCurrentProtocol;
    sProtocolCfg.ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    sProtocolCfg.bDQS      = pState->sHalCfg.sDevCfg.bDQS;

    if (pQuirks && pQuirks->soft_reset)
    {
        if (pQuirks->soft_reset(pFlash, &sProtocolCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    if (xspi_nor_reg_write_exec(pFlash,
                                AM_DEVICES_XSPI_NOR_CMD_RESET_ENABLE,
                                0u,
                                NULL,
                                0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_reg_write_exec(pFlash,
                                AM_DEVICES_XSPI_NOR_CMD_RESET_MEMORY,
                                0u,
                                NULL,
                                0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pQuirks && pQuirks->post_reset)
    {
        if (pQuirks->post_reset(pFlash, &sProtocolCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Enter 4-byte address mode based on device policy.
//!
//! @param pFlash is the device handle.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_enter_4byte(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bUse4ByteAddr)
    {
        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }

    switch (pState->pDevice->eEnter4ByteMethod)
    {
        case AM_DEVICES_XSPI_NOR_ENTER_4BYTE_NONE:
            return AM_DEVICES_XSPI_STATUS_SUCCESS;

        case AM_DEVICES_XSPI_NOR_ENTER_4BYTE_06_B7:
            if (xspi_nor_write_enable(pFlash))
            {
                return AM_DEVICES_XSPI_STATUS_ERROR;
            }
            break;

        case AM_DEVICES_XSPI_NOR_ENTER_4BYTE_B7:
            break;

        case AM_DEVICES_XSPI_NOR_ENTER_4BYTE_DEDICATED_4B_OPCODES:
        case AM_DEVICES_XSPI_NOR_ENTER_4BYTE_ALWAYS_4BYTE:
            return AM_DEVICES_XSPI_STATUS_SUCCESS;

        default:
            return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_reg_write_exec(pFlash,
                                AM_DEVICES_XSPI_NOR_CMD_ENTER_4BYTE,
                                0u,
                                NULL,
                                0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_wait_ready(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
// NOR Operation Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Read memory data from flash array using current protocol settings.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved READ command attributes.
//! @param ui32Address is start address in flash.
//! @param pData points to destination buffer.
//! @param ui32NumBytes is number of bytes to read.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_read_data(xspi_nor_state_t *pState,
                   const am_devices_xspi_nor_cmd_info_t *pCmd,
                   uint32_t ui32Address,
                   uint8_t *pData,
                   uint32_t ui32NumBytes)
{
    xspi_nor_transfer_cfg_t sXferCfg;

    if (xspi_nor_data_read_cfg_build(pState, pCmd, &sXferCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sXferCfg.ui32Address  = ui32Address;
    sXferCfg.pData        = pData;
    sXferCfg.ui32NumBytes = ui32NumBytes;

    return xspi_nor_data_transfer(pState, &sXferCfg);
}

//*****************************************************************************
//
//! @brief Program one data chunk to flash using page-program command.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved PAGE_PROGRAM command attributes.
//! @param ui32Address is destination address in flash.
//! @param pData points to source buffer.
//! @param ui32NumBytes is number of bytes to write.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_page_program(xspi_nor_state_t *pState,
                      const am_devices_xspi_nor_cmd_info_t *pCmd,
                      uint32_t ui32Address,
                      uint8_t *pData,
                      uint32_t ui32NumBytes)
{
    xspi_nor_transfer_cfg_t sXferCfg;

    if (xspi_nor_data_program_cfg_build(pState, pCmd, &sXferCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sXferCfg.ui32Address  = ui32Address;
    sXferCfg.pData        = pData;
    sXferCfg.ui32NumBytes = ui32NumBytes;

    return xspi_nor_data_transfer(pState, &sXferCfg);
}

//*****************************************************************************
//
//! @brief Read JEDEC ID using standard command/profile path.
//!
//! @param pState is the NOR runtime state.
//! @param pCmd points to resolved READ_ID command attributes.
//! @param pID points to destination ID buffer.
//! @param ui32NumBytes is requested ID length in bytes.
//! @param pOutLen points to returned valid ID length.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_read_jedec_id_standard(xspi_nor_state_t *pState,
                                const am_devices_xspi_nor_cmd_info_t *pCmd,
                                uint8_t *pID,
                                uint32_t ui32NumBytes,
                                uint32_t *pOutLen)
{
    uint32_t                ui32XferBytes;
    xspi_nor_jedec_buf_t    sIDBuf = {0};
    xspi_nor_transfer_cfg_t sXferCfg;
    uint32_t                ui32Status;

    if (!pID || (ui32NumBytes == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_id_read_cfg_build(pState, pCmd, &sXferCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = xspi_nor_transfer_size_align_up(pState, ui32NumBytes, &ui32XferBytes);
    if ((ui32Status != AM_DEVICES_XSPI_STATUS_SUCCESS) ||
        (ui32XferBytes > sizeof(sIDBuf.aui8)))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    sXferCfg.ui32Address  = 0u;
    sXferCfg.pData        = sIDBuf.aui8;
    sXferCfg.ui32NumBytes = ui32XferBytes;
    ui32Status            = xspi_nor_transfer_custom_mode(pState,
                                                          pState->eXferMode,
                                                          &sXferCfg);

    if ((ui32Status == AM_DEVICES_XSPI_STATUS_SUCCESS) && pOutLen)
    {
        memcpy(pID, sIDBuf.aui8, ui32NumBytes);
        *pOutLen = ui32NumBytes;
    }
    else if (ui32Status == AM_DEVICES_XSPI_STATUS_SUCCESS)
    {
        memcpy(pID, sIDBuf.aui8, ui32NumBytes);
    }

    return ui32Status;
}

//*****************************************************************************
//
//! @brief Select best erase type for current address and remaining bytes.
//!
//! @param pState is the NOR runtime state.
//! @param ui32Address is current erase start address.
//! @param ui32Remain is number of bytes still to erase.
//!
//! @return Pointer to chosen erase type, or NULL if no aligned type is usable.
//
//*****************************************************************************
static const am_devices_xspi_nor_erase_type_t *
xspi_nor_find_erase_type(const xspi_nor_state_t *pState,
                         uint32_t ui32Address,
                         uint32_t ui32Remain)
{
    const am_devices_xspi_nor_erase_type_t *pBest = NULL;
    const am_devices_xspi_nor_erase_type_t *pTail = NULL;

    if (!pState->pDevice->pEraseTypes)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < pState->pDevice->ui32NumEraseTypes; i++)
    {
        const am_devices_xspi_nor_erase_type_t *pType = &pState->pDevice->pEraseTypes[i];

        if (pType->ui32Size == 0u)
        {
            continue;
        }
        if ((ui32Address & (pType->ui32Size - 1u)) != 0u)
        {
            continue;
        }
        if (pType->ui32Size <= ui32Remain)
        {
            if (!pBest || (pType->ui32Size > pBest->ui32Size))
            {
                pBest = pType;
            }
        }
        else if (!pTail || (pType->ui32Size < pTail->ui32Size))
        {
            pTail = pType;
        }
    }

    return pBest ? pBest : pTail;
}

//*****************************************************************************
//
// Configuration And Probe Helpers
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Apply memory-map (AIP) configuration to HAL and runtime info.
//!
//! @param pState is the NOR runtime state.
//! @param pCfg points to memory-map configuration.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_mem_map_config(xspi_nor_state_t *pState,
                        const am_devices_xspi_memmap_cfg_t *pCfg)
{
    am_hal_xspimc_aip_cfg_t *pAIPCfg = &pState->sHalCfg.sAIPCfg;

    if (pCfg->bEnable)
    {
        pAIPCfg->ui32AIPBaseAddr         = pCfg->ui32BaseAddr;
        pAIPCfg->ui32MappedDevBaseOffset = pCfg->ui32MappedDevBaseOffset;
        pAIPCfg->ui32AIPSize             = pCfg->ui32Size;
        pAIPCfg->bPrefetch               = pCfg->bPrefetch;
    }
    pAIPCfg->ui8WindowNum = pCfg->ui8WindowNum;
    pAIPCfg->bAIPEnable   = pCfg->bEnable;

    if (AM_HAL_STATUS_SUCCESS !=
        am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                              AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                              pAIPCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->sInfo.ui32BaseAddr = pAIPCfg->ui32AIPBaseAddr;

    if (pCfg->bEnable)
    {
        /*am_hal_xspimc_security_cfg_t *pSecCfg = &pState->sHalCfg.sSecurityCfg;

        pSecCfg->ui32BaseAddr = pCfg->ui32EncryptBaseAddr;
        pSecCfg->ui32Size     = pCfg->ui32EncryptSize;
        pSecCfg->bEnable      = (pCfg->ui32EncryptSize != 0u);
        pSecCfg->key          = pCfg->ui32EncryptKey;

        if (AM_HAL_STATUS_SUCCESS !=
            am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                  AM_HAL_XSPIMC_CONTROL_SECURITY_CONFIG,
                                  pSecCfg))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }*/
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Initialize default AIP configuration values.
//!
//! @param pState is the NOR runtime state.
//
//*****************************************************************************
static void
xspi_nor_default_aipcfg_set(xspi_nor_state_t *pState)
{
    am_hal_xspimc_aip_cfg_t *pAIPCfg;

    pAIPCfg = &pState->sHalCfg.sAIPCfg;
    memset(pAIPCfg, 0, sizeof(am_hal_xspimc_aip_cfg_t));

    pAIPCfg->bAIPEnable  = false;
    pAIPCfg->ePermission = AM_HAL_XSPIMC_AIP_RO;
}

//*****************************************************************************
//
//! @brief Initialize default security configuration values.
//!
//! @param pState is the NOR runtime state.
//
//*****************************************************************************
static void
xspi_nor_default_securitycfg_set(xspi_nor_state_t *pState)
{
    /*am_hal_xspimc_security_cfg_t *pSecCfg;

    pSecCfg = &pState->sHalCfg.sSecurityCfg;
    memset(pSecCfg, 0, sizeof(am_hal_xspimc_security_cfg_t));

    pSecCfg->bEnable = false;*/
}

//*****************************************************************************
//
//! @brief Initialize default timing configuration values.
//!
//! @param pState is the NOR runtime state.
//
//*****************************************************************************
static void
xspi_nor_default_timingcfg_set(xspi_nor_state_t *pState)
{
    am_hal_xspimc_timing_cfg_t *pTimingCfg;

    pTimingCfg = &pState->sHalCfg.sTimingCfg;
    memset(pTimingCfg, 0, sizeof(am_hal_xspimc_timing_cfg_t));

    pTimingCfg->ui8DataDelay = 0x00;
    pTimingCfg->ui8DQSDelay  = 0x1F;
}

//*****************************************************************************
//
//! @brief Initialize default HAL configuration blocks for NOR runtime.
//!
//! @param pState is the NOR runtime state.
//
//*****************************************************************************
static void
xspi_nor_default_set(xspi_nor_state_t *pState)
{
    xspi_nor_default_aipcfg_set(pState);
    xspi_nor_default_securitycfg_set(pState);
    xspi_nor_default_timingcfg_set(pState);
}

//*****************************************************************************
//
//! @brief Convert user memory-map config into HAL AIP configuration.
//!
//! @param pState is the NOR runtime state.
//! @param pCfg points to user flash config.
//! @param pAipCfg points to HAL AIP config output.
//
//*****************************************************************************
static void
xspi_nor_cov_aipcfg(xspi_nor_state_t *pState,
                    const am_devices_xspi_norflash_cfg_t *pCfg,
                    am_hal_xspimc_aip_cfg_t *pAipCfg)
{
    pAipCfg->ui32AIPBaseAddr         = pCfg->sMemMap.ui32BaseAddr;
    pAipCfg->ui32MappedDevBaseOffset = pCfg->sMemMap.ui32MappedDevBaseOffset;
    pAipCfg->ui32AIPSize             = pCfg->sMemMap.ui32Size;
    pAipCfg->bAIPEnable              = pCfg->sMemMap.bEnable;
    pAipCfg->ui8WindowNum            = pCfg->sMemMap.ui8WindowNum;
    pAipCfg->bPrefetch               = pCfg->sMemMap.bPrefetch;
}

//*****************************************************************************
//
//! @brief Get JEDEC read length requested by current device descriptor.
//!
//! @param pState is the NOR runtime state.
//!
//! @return JEDEC ID read length in bytes.
//
//*****************************************************************************
static uint32_t
xspi_nor_jedec_id_len_get(const xspi_nor_state_t *pState)
{
    uint32_t ui32Len = 3u;

    if (pState && (pState->pDevice->ui8JedecIdReadLen != 0u))
    {
        ui32Len = pState->pDevice->ui8JedecIdReadLen;
    }

    if (ui32Len > XSPI_NOR_MAX_JEDEC_ID_LEN)
    {
        ui32Len = XSPI_NOR_MAX_JEDEC_ID_LEN;
    }

    return ui32Len;
}

//*****************************************************************************
//
//! @brief Parse default compact JEDEC ID fields from raw buffer.
//!
//! @param pRawId points to raw JEDEC ID bytes.
//! @param ui32IdLen is number of valid bytes in raw ID buffer.
//! @param pInfo points to parsed device info output.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_parse_jedec_id_default(const uint8_t *pRawId,
                                uint32_t ui32IdLen,
                                am_devices_xspi_info_t *pInfo)
{
    if (!pRawId || !pInfo || (ui32IdLen == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pInfo->ui8VendorId = pRawId[0];
    pInfo->ui8DeviceId = (ui32IdLen > 1u) ? pRawId[1] : 0u;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Validate parsed JEDEC ID against selected device descriptor.
//!
//! @param pDevice points to selected device descriptor.
//! @param pInfo points to parsed JEDEC ID information.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_jedec_id_validate(const am_devices_xspi_nor_device_t *pDevice,
                           const am_devices_xspi_info_t *pInfo)
{
    if (!pDevice || !pInfo)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((pDevice->ui8VendorId != 0u) && (pInfo->ui8VendorId != pDevice->ui8VendorId))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((pDevice->ui8DeviceId != 0u) && (pInfo->ui8DeviceId != pDevice->ui8DeviceId))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Read JEDEC ID via quirk path or standard command path.
//!
//! @param pFlash is the device handle.
//! @param pID points to destination ID buffer.
//! @param ui32NumBytes is requested ID length in bytes.
//! @param pInfo points to parsed compact ID output.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_read_jedec_id(am_device_xspi_periph_handle_t *pFlash,
                       uint8_t *pID,
                       uint32_t ui32NumBytes,
                       am_devices_xspi_info_t *pInfo)
{
    xspi_nor_state_t                   *pState = (xspi_nor_state_t *)pFlash->pData;
    const am_devices_xspi_nor_quirks_t *pQuirks;
    uint32_t                           ui32ReadLen;
    uint32_t                           ui32Status;

    pQuirks = pState->pDevice->pQuirks;

    if (pQuirks && pQuirks->read_jedec_id)
    {
        ui32Status = pQuirks->read_jedec_id(pFlash,
                                            pID,
                                            ui32NumBytes,
                                            pInfo);
    }
    else
    {
        xspi_nor_mode_ctx_t sModeCtx = {0};

        if (xspi_nor_command_mode_prepare(pFlash,
                                          AM_DEVICES_XSPI_NOR_CMD_READ_ID,
                                          AM_HAL_XSPIMC_XFER_READ,
                                          ui32NumBytes,
                                          &sModeCtx))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        ui32Status = xspi_nor_read_jedec_id_standard(pState,
                                                     sModeCtx.pCmd,
                                                     pID,
                                                     ui32NumBytes,
                                                     &ui32ReadLen);
        if (ui32Status == AM_DEVICES_XSPI_STATUS_SUCCESS)
        {
            ui32Status = xspi_nor_parse_jedec_id_default(pID, ui32ReadLen, pInfo);
        }

        if (xspi_nor_command_mode_restore(pFlash, &sModeCtx))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
    }

    return ui32Status;
}

//*****************************************************************************
//
//! @brief Acquire peripheral ownership and enable the xSPI device.
//!
//! @param pFlash is the device handle.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_device_acquire(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;

    if (AM_HAL_STATUS_SUCCESS != am_hal_periph_acquire(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_handle_t *pController = pState->pBus;

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, true);

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_enable(pState->pBus, &pState->sDeviceID))
    {
        (void)am_hal_periph_release(pFlash);
        am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

//*****************************************************************************
//
//! @brief Disable the xSPI device and release peripheral ownership.
//!
//! @param pFlash is the device handle.
//!
//! @return Driver status code.
//
//*****************************************************************************
static uint32_t
xspi_nor_device_release(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState    = (xspi_nor_state_t *)pFlash->pData;
    uint32_t         ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_disable(pState->pBus, &pState->sDeviceID))
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_handle_t *pController = pState->pBus;

    am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);

    if (AM_HAL_STATUS_SUCCESS != am_hal_periph_release(pFlash))
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return ui32Status;
}

//*****************************************************************************
//
//! @brief Force cleanup path and return generic driver error.
//!
//! @param pFlash is the device handle.
//!
//! @return Always returns driver error status.
//
//*****************************************************************************
static uint32_t
xspi_nor_device_error_ret(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t       *pState      = (xspi_nor_state_t *)pFlash->pData;
    am_hal_periph_handle_t *pController = pState->pBus;

    (void)am_hal_xspimc_disable(pState->pBus, &pState->sDeviceID);
    am_device_xspi_cpu_vote(pController->ui8ModuleNum, false);
    (void)am_hal_periph_release(pFlash);

    return AM_DEVICES_XSPI_STATUS_ERROR;
}

//*****************************************************************************
//
// Public API Implementations
//
//*****************************************************************************

uint32_t
am_devices_xspi_nor_command_write(am_device_xspi_periph_handle_t *pFlash,
                                  uint16_t ui16Cmd,
                                  uint8_t ui8CmdLen,
                                  uint8_t ui8AddrLen,
                                  uint32_t ui32Addr,
                                  const uint8_t *pData,
                                  uint32_t ui32NumBytes,
                                  uint8_t ui8TxDummy)
{
    xspi_nor_state_t       *pState;
    xspi_nor_transfer_cfg_t sXferCfg;

    if (ui8CmdLen == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    sXferCfg.eDir          = AM_HAL_XSPIMC_XFER_WRITE;
    sXferCfg.ui16Cmd       = ui16Cmd;
    sXferCfg.ui8CmdLen     = ui8CmdLen;
    sXferCfg.ui8AddrLen    = ui8AddrLen;
    sXferCfg.ui8TxDummy    = ui8TxDummy;
    sXferCfg.ui8RxDummy    = 0u;
    sXferCfg.ui8ModeBitLen = 0u;
    sXferCfg.ui32ModeBits  = 0u;
    sXferCfg.ui32Address   = ui32Addr;
    sXferCfg.pData         = (uint8_t *)pData;
    sXferCfg.ui32NumBytes  = ui32NumBytes;

    return xspi_nor_transfer_custom_mode(pState,
                                         pState->eXferMode,
                                         &sXferCfg);
}

uint32_t
am_devices_xspi_nor_command_read(am_device_xspi_periph_handle_t *pFlash,
                                 uint16_t ui16Cmd,
                                 uint8_t ui8CmdLen,
                                 uint8_t ui8AddrLen,
                                 uint32_t ui32Addr,
                                 uint8_t *pData,
                                 uint32_t ui32NumBytes,
                                 uint8_t ui8RxDummy)
{
    xspi_nor_state_t       *pState;
    xspi_nor_transfer_cfg_t sXferCfg;

    if (!pData || (ui8CmdLen == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    sXferCfg.eDir          = AM_HAL_XSPIMC_XFER_READ;
    sXferCfg.ui16Cmd       = ui16Cmd;
    sXferCfg.ui8CmdLen     = ui8CmdLen;
    sXferCfg.ui8AddrLen    = ui8AddrLen;
    sXferCfg.ui8TxDummy    = 0u;
    sXferCfg.ui8RxDummy    = ui8RxDummy;
    sXferCfg.ui8ModeBitLen = 0u;
    sXferCfg.ui32ModeBits  = 0u;
    sXferCfg.ui32Address   = ui32Addr;
    sXferCfg.pData         = pData;
    sXferCfg.ui32NumBytes  = ui32NumBytes;

    if (ui8AddrLen != 0u)
    {
        return xspi_nor_transfer_read_split_mode(pState,
                                                 pState->eXferMode,
                                                 &sXferCfg);
    }

    return xspi_nor_transfer_custom_mode(pState,
                                         pState->eXferMode,
                                         &sXferCfg);
}

uint32_t
am_devices_xspi_nor_write_enable(am_device_xspi_periph_handle_t *pFlash)
{
    return xspi_nor_write_enable(pFlash);
}

uint32_t
am_devices_xspi_nor_wait_ready(am_device_xspi_periph_handle_t *pFlash, uint32_t ui32TimeoutUs)
{
    return xspi_nor_wait_ready(pFlash, ui32TimeoutUs);
}

uint32_t
am_devices_xspi_nor_set_xfer_mode(am_device_xspi_periph_handle_t *pFlash,
                                  am_hal_xspimc_xfer_mode_t eXferMode)
{
    xspi_nor_state_t *pState;

    if ((eXferMode != AM_HAL_XSPIMC_PIO_XFER) &&
        (eXferMode != AM_HAL_XSPIMC_DMA_XFER))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState            = (xspi_nor_state_t *)pFlash->pData;
    pState->eXferMode = eXferMode;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_read_jedec_id(am_device_xspi_periph_handle_t *pFlash,
                                  uint8_t *pRawId,
                                  uint32_t ui32BufLen)
{
    xspi_nor_state_t *pState;
    uint32_t          ui32ReadLen;

    if (!pRawId || (ui32BufLen == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32ReadLen = xspi_nor_jedec_id_len_get(pState);
    if (ui32ReadLen > ui32BufLen)
    {
        ui32ReadLen = ui32BufLen;
    }

    if (xspi_nor_read_jedec_id(pFlash, pRawId, ui32ReadLen, &pState->sInfo))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_read_sfdp(am_device_xspi_periph_handle_t *pFlash,
                              uint32_t ui32Addr,
                              uint8_t *pData,
                              uint32_t ui32NumBytes)
{
    xspi_nor_state_t       *pState;
    xspi_nor_mode_ctx_t     sModeCtx = {0};
    xspi_nor_transfer_cfg_t sXferCfg;
    uint32_t                ui32Status;

    if (!pData || (ui32NumBytes == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = xspi_nor_command_mode_prepare(pFlash,
                                               AM_DEVICES_XSPI_NOR_CMD_READ_SFDP,
                                               AM_HAL_XSPIMC_XFER_READ,
                                               ui32NumBytes,
                                               &sModeCtx);
    if (ui32Status)
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_sfdp_read_cfg_build(pState,
                                     sModeCtx.pCmd,
                                     &sXferCfg))
    {
        ui32Status = AM_DEVICES_XSPI_STATUS_ERROR;
        goto read_sfdp_cleanup;
    }

    sXferCfg.ui32Address  = ui32Addr;
    sXferCfg.pData        = pData;
    sXferCfg.ui32NumBytes = ui32NumBytes;
    ui32Status            = xspi_nor_transfer_read_split_mode(pState,
                                                              pState->eXferMode,
                                                              &sXferCfg);
read_sfdp_cleanup:
    if (xspi_nor_command_mode_restore(pFlash, &sModeCtx))
    {
        return xspi_nor_device_error_ret(pFlash);
    }
    if (ui32Status)
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_apply_protocol(am_device_xspi_periph_handle_t *pFlash,
                                   am_devices_xspi_nor_protocol_e eProtocol,
                                   uint32_t ui32Freq,
                                   bool bDQS)
{
    xspi_nor_state_t                              *pState;
    const am_devices_xspi_nor_cmd_set_t           *pSet;
    const am_devices_xspi_nor_jedec_cmd_profile_t *pJEDECProfile;
    const am_devices_xspi_nor_jedec_cmd_info_t    *pJEDEC = NULL;
    const am_devices_xspi_nor_cmd_info_t          *pRead;
    const am_devices_xspi_nor_cmd_info_t          *pProg;
    am_hal_xspimc_dev_cfg_t                       *pDevCfg;
    uint8_t                                        ui8ModeBitLen;
    uint8_t                                        ui8AddrLen;
    uint8_t                                        ui8RxDummy;
    uint32_t                                       ui32ModeBits;

    if (!pFlash || !pFlash->pData)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState        = (xspi_nor_state_t *)pFlash->pData;
    pSet          = xspi_nor_cmd_set_resolve(pState->pDevice, eProtocol);
    pJEDECProfile = xspi_nor_jedec_profile_resolve(pState->pDevice, eProtocol);
    pDevCfg       = &pState->sHalCfg.sDevCfg;

    if (!pSet)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (pJEDECProfile)
    {
        pJEDEC = &pJEDECProfile->sCmdInfo;
    }

    pRead = &pSet->asCmdInfo[AM_DEVICES_XSPI_NOR_CMD_READ];
    pProg = &pSet->asCmdInfo[AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM];
    if (pRead->ui16Cmd == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui8AddrLen    = xspi_nor_cmd_addr_len_get(pState, pRead);
    ui8RxDummy    = xspi_nor_read_dummy_get(pRead, pJEDEC);
    ui8ModeBitLen = pRead->ui8ModeBitLen;
    ui32ModeBits  = pRead->ui32ModeBits;

    pDevCfg->ui32Freq        = ui32Freq;
    pDevCfg->eIOMode         = pSet->eIOMode;
    pDevCfg->eDataRate       = pSet->eDataRate;
    pDevCfg->eCpp            = AM_HAL_XSPIMC_CPP_MODE_0;
    pDevCfg->bDQS            = bDQS;
    pDevCfg->ui32ReadCmd     = xspi_nor_cmd_opcode_get(pState, pRead);
    pDevCfg->ui32WriteCmd    = (pProg->ui16Cmd != 0u) ?
                               xspi_nor_cmd_opcode_get(pState, pProg) : 0u;
    pDevCfg->ui32Mode        = ui32ModeBits;
    pDevCfg->ui8CmdLen       = pRead->ui8CmdLen;
    pDevCfg->ui8AddrLen      = ui8AddrLen;
    pDevCfg->ui8AddrAlign    = 1u;
    pDevCfg->ui8ModeLen      = ui8ModeBitLen;
    pDevCfg->ui8RxDummy      = ui8RxDummy;
    pDevCfg->ui8TxDummy      = (pProg->ui16Cmd != 0u) ? pProg->ui8TxDummy : 0u;
    pDevCfg->sCETiming       = pState->pDevice->sCETiming;
    pDevCfg->eDeviceType     = AM_HAL_XSPIMC_FLASH_DEVICE;
    pDevCfg->ui32MemBoundary = 0;

    if (AM_HAL_STATUS_SUCCESS !=
        am_hal_xspimc_device_configure(pState->pBus, &pState->sDeviceID, pDevCfg))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    am_hal_periph_handle_t *pBus = pState->pBus;

    am_bsp_xspi_pins_enable(pBus->ui8ModuleNum, pFlash->ui8ModuleNum, pDevCfg->eIOMode);

    pState->pCmdSet          = pSet;
    pState->pJEDECCmdProfile = pJEDECProfile;
    pState->eCurrentProtocol = eProtocol;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_protocol_cfg_get(am_device_xspi_periph_handle_t *pFlash,
                                     am_devices_xspi_nor_protocol_cfg_t *pCfg)
{
    xspi_nor_state_t *pState;

    if (!pFlash || !pFlash->pData || !pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState          = (xspi_nor_state_t *)pFlash->pData;
    pCfg->eProtocol = pState->eCurrentProtocol;
    pCfg->ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    pCfg->bDQS      = pState->sHalCfg.sDevCfg.bDQS;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_init(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState;
    uint32_t         ui32Status;

    if (!pFlash->pData ||
        (pFlash->uintSize < sizeof(xspi_nor_state_t)))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_preinit(pFlash);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;
    memset(pState, 0, sizeof(*pState));
    pState->eXferMode        = AM_HAL_XSPIMC_PIO_XFER;
    pState->eCurrentProtocol = AM_DEVICES_XSPI_NOR_PROTO_INVALID;

    pState->sDeviceID = (am_hal_xspimc_dev_id)
    {
        .ui8HWCENum = pFlash->ui8ModuleNum,
        .eCEMode    = AM_HAL_XSPIMC_HARDWARE_CE,
    };

    {
        const am_devices_xspi_deps_t *pDeps = (const am_devices_xspi_deps_t *)pFlash->pDeps;
        if (!pDeps || !pDeps->pXSPI)
        {
            (void)am_hal_periph_postdeinit(pFlash);
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        pState->pBus = pDeps->pXSPI;
    }

    pState->ui32PageSize = 256u;
    pState->sInfo.sCE    = pState->sDeviceID;

    xspi_nor_default_set(pState);

    pFlash->state.bInitialized = true;
    pFlash->state.ui8InitRet   = AM_DEVICES_XSPI_STATUS_SUCCESS;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_deinit(am_device_xspi_periph_handle_t *pFlash)
{
    xspi_nor_state_t *pState = (xspi_nor_state_t *)pFlash->pData;
    uint32_t          ui32Status;

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_xspimc_power_control(pState->pBus,
                                             &pState->sDeviceID,
                                             AM_HAL_XSPIMC_POWER_REQUEST_OFF);
    if (ui32Status)
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32Status = am_hal_periph_postdeinit(pFlash);
    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pFlash->state.bInitialized = false;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_config(am_device_xspi_periph_handle_t *pFlash,
                           const am_devices_xspi_norflash_cfg_t *pConfig,
                           const am_devices_xspi_nor_device_t *pDevice)
{
    xspi_nor_state_t                              *pState;
    const am_devices_xspi_nor_jedec_cmd_profile_t *pJEDECProfile;
    am_devices_xspi_nor_protocol_e                 eTargetProtocol;
    am_devices_xspi_nor_protocol_e                 eDefaultProtocol;
    uint8_t                                        ui8ID[XSPI_NOR_MAX_JEDEC_ID_LEN] = {0};
    uint32_t                                       ui32IDLen                        = 0u;
    uint32_t                                       ui32TargetFreq;
    uint32_t                                       ui32BootstrapFreq;
    uint32_t                                       ui32Status;

    if (!pConfig || !pDevice)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    eTargetProtocol = am_devices_xspi_nor_protocol_from_hal(pConfig->eIOMode,
                                                            pConfig->eDataRate);
    if (eTargetProtocol == AM_DEVICES_XSPI_NOR_PROTO_INVALID)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    eDefaultProtocol = pDevice->eDefaultProtocol;
    if (eDefaultProtocol == AM_DEVICES_XSPI_NOR_PROTO_INVALID)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (!xspi_nor_proto_supported(pDevice, eTargetProtocol) ||
        !xspi_nor_proto_supported(pDevice, eDefaultProtocol))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    ui32TargetFreq = pConfig->ui32Freq;
    if (ui32TargetFreq == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }
    ui32BootstrapFreq = (ui32TargetFreq > AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ) ?
                        AM_DEVICES_XSPI_NOR_BOOTSTRAP_FREQ_HZ : ui32TargetFreq;

    pJEDECProfile = xspi_nor_jedec_profile_resolve(pDevice, eTargetProtocol);
    if (!pJEDECProfile)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState->pDevice = pDevice;

    if (pState->bUse4ByteAddr != (pDevice->bSupport4ByteAddress && pDevice->bUse4ByteOpcodes))
    {
        //force reapply controller configuraiton
        pState->eCurrentProtocol = AM_DEVICES_XSPI_NOR_PROTO_INVALID;
    }
    pState->bUse4ByteAddr = pDevice->bSupport4ByteAddress && pDevice->bUse4ByteOpcodes;

    // Override 4-byte address setting if the target JEDEC profile
    // indicates that 4-byte addressing should be used.
    if (pJEDECProfile->sCmdInfo.bUses4ByteAddr)
    {
        pState->bUse4ByteAddr = true;
    }

    pState->ui32PageSize = (pDevice->ui32PageSize != 0u) ? pDevice->ui32PageSize : 256u;

    if (am_devices_xspi_nor_apply_protocol(pFlash,
                                           eDefaultProtocol,
                                           ui32BootstrapFreq,
                                           false))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       &pState->sHalCfg.sTimingCfg))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_reset(pFlash))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_enter_4byte(pFlash))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    ui32IDLen  = xspi_nor_jedec_id_len_get(pState);
    ui32Status = xspi_nor_read_jedec_id(pFlash,
                                        ui8ID,
                                        ui32IDLen,
                                        &pState->sInfo);
    if (ui32Status)
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    ui32Status = xspi_nor_jedec_id_validate(pDevice, &pState->sInfo);
    if (ui32Status)
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (pState->pDevice->pQuirks && pState->pDevice->pQuirks->post_init)
    {
        if (pState->pDevice->pQuirks->post_init(pFlash))
        {
            return xspi_nor_device_error_ret(pFlash);
        }
    }

    pState->sInfo.ui32DeviceSizeKb = pDevice->ui32DeviceSizeKb;
    pState->sInfo.sCE              = pState->sDeviceID;

    if (xspi_nor_mode_switch(pFlash, eTargetProtocol))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_controller_config_apply_if_needed(pFlash,
                                                   eTargetProtocol,
                                                   ui32TargetFreq,
                                                   pConfig->bDQS))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    xspi_nor_cov_aipcfg(pState, pConfig, &pState->sHalCfg.sAIPCfg);
    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_AIP_CONFIG,
                                                       &pState->sHalCfg.sAIPCfg))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    pState->sInfo.ui32BaseAddr = pState->sHalCfg.sAIPCfg.ui32AIPBaseAddr;

    //
    // Read-while-write configuration (optional, opt-in via pConfig).
    //
    pState->pRWWCfg     = NULL;
    pState->bRWWEnabled = false;

    if (pConfig->bEnableReadWhileWrite)
    {
#if (AM_DEVICES_XSPI_NOR_ENABLE_AIP_READ != 0)
        // RWW requires hardware CE and an enabled, readable AIP window.
        if ((pState->sDeviceID.eCEMode != AM_HAL_XSPIMC_HARDWARE_CE) ||
            !pState->sHalCfg.sAIPCfg.bAIPEnable)
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        const am_devices_xspi_nor_rww_cfg_t *pRWWProfile =
            xspi_nor_rww_profile_resolve(pDevice, eTargetProtocol);
        if (!pRWWProfile)
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        am_hal_xspimc_sus_res_cfg_t sSusResCfg;
        if (xspi_nor_rww_hal_cfg_build(pState, pRWWProfile, &sSusResCfg) !=
            AM_DEVICES_XSPI_STATUS_SUCCESS)
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        if (AM_HAL_STATUS_SUCCESS !=
            am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                  AM_HAL_XSPIMC_CONTROL_READ_WHILE_WRITE_CONFIG,
                                  &sSusResCfg))
        {
            // Roll back any partial RWW hardware state before failing.
            sSusResCfg.bAutoSusResEnable = false;
            (void)am_hal_xspimc_control(pState->pBus, &pState->sDeviceID,
                                        AM_HAL_XSPIMC_CONTROL_READ_WHILE_WRITE_CONFIG,
                                        &sSusResCfg);
            return xspi_nor_device_error_ret(pFlash);
        }

        pState->pRWWCfg     = pRWWProfile;
        pState->bRWWEnabled = true;
#else
        // AIP-backed read support is compile-time disabled.
        return xspi_nor_device_error_ret(pFlash);
#endif
    }

    pState->bConfigured = true;

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_get_info(am_device_xspi_periph_handle_t *pFlash, am_devices_xspi_info_t *pInfo)
{
    xspi_nor_state_t *pState;

    if (!pInfo)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;
    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    *pInfo = pState->sInfo;

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_read(am_device_xspi_periph_handle_t *pFlash,
                         uint32_t ui32Addr,
                         uint32_t ui32Size,
                         void *pData)
{
    xspi_nor_state_t                   *pState;
    am_devices_xspi_nor_protocol_cfg_t  sProtocolCfg;
    xspi_nor_mode_ctx_t                 sModeCtx;
    const uint32_t                      ui32StartAddr = ui32Addr;
    const uint32_t                      ui32TotalSize = ui32Size;
    uint8_t                            *pui8Data      = (uint8_t *)pData;
    uint8_t                            *pui8DataBase  = (uint8_t *)pData;
    uint32_t                            ui32Read      = ui32Size;

    if (!pui8Data || (ui32Size == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

#if (AM_DEVICES_XSPI_NOR_ENABLE_AIP_READ != 0)
    //
    // Read-while-write AIP-backed read branch.
    //
    // When RWW is active, the writer owns the peripheral lock and the custom
    // SPI command path is not the concurrent path. Reads that fall entirely
    // within the configured enabled AIP window are served directly from the
    // memory-mapped aperture without acquiring the lock.
    //
    if (pState->bRWWEnabled && pState->sHalCfg.sAIPCfg.bAIPEnable)
    {
        const uint32_t ui32AipBase    = pState->sHalCfg.sAIPCfg.ui32AIPBaseAddr;
        const uint32_t ui32DevBaseOff = pState->sHalCfg.sAIPCfg.ui32MappedDevBaseOffset;
        const uint32_t ui32AipSize    = pState->sHalCfg.sAIPCfg.ui32AIPSize;

        // Overflow-safe device bounds check.
        if (ui32Addr > (0xFFFFFFFFu - ui32Size))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }
        uint32_t ui32EndAddr = ui32Addr + ui32Size;

        // The complete source range must lie within the mapped device region
        // covered by the AIP window.
        if ((ui32Addr < ui32DevBaseOff) ||
            (ui32EndAddr > (ui32DevBaseOff + ui32AipSize)))
        {
            return AM_DEVICES_XSPI_STATUS_ERROR;
        }

        // cpu_address = AIP_base + (device_address - mapped_device_base_offset)
        uint32_t ui32CpuAddr = ui32AipBase + (ui32Addr - ui32DevBaseOff);
        memcpy(pui8Data, (const void *)ui32CpuAddr, ui32Size);

        return AM_DEVICES_XSPI_STATUS_SUCCESS;
    }
#endif

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sProtocolCfg.eProtocol = pState->eCurrentProtocol;
    sProtocolCfg.ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    sProtocolCfg.bDQS      = pState->sHalCfg.sDevCfg.bDQS;

    if (xspi_nor_mem_mode_prepare(pFlash, AM_DEVICES_XSPI_NOR_CMD_READ, &sModeCtx))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (pState->pDevice->pQuirks && pState->pDevice->pQuirks->pre_read)
    {
        if (pState->pDevice->pQuirks->pre_read(pFlash,
                                               &sProtocolCfg,
                                               ui32StartAddr,
                                               ui32TotalSize))
        {
            return xspi_nor_device_error_ret(pFlash);
        }
    }

    while (ui32Read != 0u)
    {
        uint32_t ui32Chunk = (ui32Read > XSPI_NOR_MAX_RW_CHUNK) ? XSPI_NOR_MAX_RW_CHUNK :
                                                                  ui32Read;

        if (xspi_nor_read_data(pState, sModeCtx.pCmd, ui32Addr, pui8Data, ui32Chunk))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        ui32Addr += ui32Chunk;
        pui8Data += ui32Chunk;
        ui32Read -= ui32Chunk;
    }

    if (pState->pDevice->pQuirks && pState->pDevice->pQuirks->post_read)
    {
        if (pState->pDevice->pQuirks->post_read(pFlash,
                                                &sProtocolCfg,
                                                ui32StartAddr,
                                                ui32TotalSize,
                                                pui8DataBase))
        {
            return xspi_nor_device_error_ret(pFlash);
        }
    }

    if (xspi_nor_mem_mode_restore(pFlash, &sModeCtx))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_write(am_device_xspi_periph_handle_t *pFlash,
                          uint32_t ui32Addr,
                          uint32_t ui32Size,
                          void *pData)
{
    xspi_nor_state_t                   *pState;
    am_devices_xspi_nor_protocol_cfg_t  sProtocolCfg;
    xspi_nor_mode_ctx_t                 sModeCtx;
    const uint32_t                      ui32StartAddr = ui32Addr;
    const uint32_t                      ui32TotalSize = ui32Size;
    uint8_t                            *pui8Data      = (uint8_t *)pData;
    const uint8_t                      *pui8DataBase  = (const uint8_t *)pData;
    uint32_t                            ui32Remain    = ui32Size;

    if (!pui8Data || (ui32Size == 0u))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    sProtocolCfg.eProtocol = pState->eCurrentProtocol;
    sProtocolCfg.ui32Freq  = pState->sHalCfg.sDevCfg.ui32Freq;
    sProtocolCfg.bDQS      = pState->sHalCfg.sDevCfg.bDQS;

    if (xspi_nor_mem_mode_prepare(pFlash, AM_DEVICES_XSPI_NOR_CMD_PAGE_PROGRAM, &sModeCtx))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (pState->pDevice->pQuirks && pState->pDevice->pQuirks->pre_write)
    {
        if (pState->pDevice->pQuirks->pre_write(pFlash,
                                                &sProtocolCfg,
                                                ui32StartAddr,
                                                ui32TotalSize,
                                                pui8DataBase))
        {
            return xspi_nor_device_error_ret(pFlash);
        }
    }

    while (ui32Remain != 0u)
    {
        uint32_t ui32PageOffset = ui32Addr % pState->ui32PageSize;
        uint32_t ui32PageLeft   = pState->ui32PageSize - ui32PageOffset;
        uint32_t ui32Chunk      = (ui32Remain > ui32PageLeft) ? ui32PageLeft : ui32Remain;

        if (xspi_nor_write_enable(pFlash))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        if (xspi_nor_page_program(pState, sModeCtx.pCmd, ui32Addr, pui8Data, ui32Chunk))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        if (xspi_nor_wait_ready(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        // Notify the HAL that this program operation completed early so it can
        // release the read-while-write holdoff for the configured AIP windows.
        xspi_nor_rww_complete_notify(pState);

        ui32Addr   += ui32Chunk;
        pui8Data   += ui32Chunk;
        ui32Remain -= ui32Chunk;
    }

    if (pState->pDevice->pQuirks && pState->pDevice->pQuirks->post_write)
    {
        if (pState->pDevice->pQuirks->post_write(pFlash,
                                                 &sProtocolCfg,
                                                 ui32StartAddr,
                                                 ui32TotalSize,
                                                 pui8DataBase))
        {
            return xspi_nor_device_error_ret(pFlash);
        }
    }

    if (xspi_nor_mem_mode_restore(pFlash, &sModeCtx))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_erase(am_device_xspi_periph_handle_t *pFlash,
                          uint32_t ui32Addr,
                          uint32_t ui32Size)
{
    xspi_nor_state_t *pState;
    uint32_t          ui32Remain = ui32Size;

    if (ui32Size == 0u)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if ((ui32Addr == 0u) &&
        (pState->pDevice->ui32DeviceSizeKb != 0u) &&
        (ui32Size >= (pState->pDevice->ui32DeviceSizeKb * 1024u)))
    {
        if (xspi_nor_write_enable(pFlash))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        if (xspi_nor_reg_write_exec(pFlash,
                                    AM_DEVICES_XSPI_NOR_CMD_CHIP_ERASE,
                                    0u,
                                    NULL,
                                    0u))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        if (xspi_nor_wait_ready(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
        {
            return xspi_nor_device_error_ret(pFlash);
        }

        xspi_nor_rww_complete_notify(pState);
    }
    else
    {
        while (ui32Remain != 0u)
        {
            const am_devices_xspi_nor_erase_type_t *pType =
                xspi_nor_find_erase_type(pState, ui32Addr, ui32Remain);
            if (!pType)
            {
                return xspi_nor_device_error_ret(pFlash);
            }

            if (xspi_nor_write_enable(pFlash))
            {
                return xspi_nor_device_error_ret(pFlash);
            }

            if (xspi_nor_reg_write_exec(pFlash,
                                        pType->eCmd,
                                        ui32Addr,
                                        NULL,
                                        0u))
            {
                return xspi_nor_device_error_ret(pFlash);
            }

            if (xspi_nor_wait_ready(pFlash, AM_DEVICES_XSPI_NOR_TIMEOUT_US))
            {
                return xspi_nor_device_error_ret(pFlash);
            }

            xspi_nor_rww_complete_notify(pState);

            ui32Addr += pType->ui32Size;
            if (ui32Remain > pType->ui32Size)
            {
                ui32Remain -= pType->ui32Size;
            }
            else
            {
                ui32Remain = 0u;
            }
        }
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_power_control(am_device_xspi_periph_handle_t *pFlash,
                                  am_devices_xspi_power_e ePowerMode,
                                  const am_devices_xspi_nor_power_cfg_t *pPowerCfg)
{
    xspi_nor_state_t *pState;

    (void)pPowerCfg;

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    switch (ePowerMode)
    {
        case AM_DEVICES_XSPI_POWER_ON:
            if (AM_HAL_STATUS_SUCCESS !=
                am_hal_xspimc_power_control(pState->pBus,
                                            &pState->sDeviceID,
                                            AM_HAL_XSPIMC_POWER_REQUEST_ON))
            {
                return xspi_nor_device_error_ret(pFlash);
            }
            break;

        case AM_DEVICES_XSPI_POWER_OFF:
            if (AM_HAL_STATUS_SUCCESS !=
                am_hal_xspimc_power_control(pState->pBus,
                                            &pState->sDeviceID,
                                            AM_HAL_XSPIMC_POWER_REQUEST_OFF))
            {
                return xspi_nor_device_error_ret(pFlash);
            }
            break;

        case AM_DEVICES_XSPI_POWER_AUTO:
        default:
            return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_mem_map_config(am_device_xspi_periph_handle_t *pFlash,
                                   const am_devices_xspi_memmap_cfg_t *pCfg)
{
    xspi_nor_state_t *pState;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (!pState->bConfigured)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (xspi_nor_mem_map_config(pState, pCfg))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    if (xspi_nor_device_release(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    return AM_DEVICES_XSPI_STATUS_SUCCESS;
}

uint32_t
am_devices_xspi_nor_timing_apply(am_device_xspi_periph_handle_t *pFlash,
                                 const am_devices_xspi_timing_t *pCfg)
{
    xspi_nor_state_t *pState;

    if (!pCfg)
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    pState = (xspi_nor_state_t *)pFlash->pData;

    if (xspi_nor_device_acquire(pFlash))
    {
        return AM_DEVICES_XSPI_STATUS_ERROR;
    }

    if (AM_HAL_STATUS_SUCCESS != am_hal_xspimc_control(pState->pBus,
                                                       &pState->sDeviceID,
                                                       AM_HAL_XSPIMC_CONTROL_TIMING_CONFIG,
                                                       pCfg))
    {
        return xspi_nor_device_error_ret(pFlash);
    }

    pState->sHalCfg.sTimingCfg = *pCfg;

    if (xspi_nor_device_release(pFlash))
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
