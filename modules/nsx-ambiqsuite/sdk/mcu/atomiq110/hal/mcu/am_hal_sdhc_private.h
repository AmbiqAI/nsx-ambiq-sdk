//*****************************************************************************
//
//! @file am_hal_sdhc_private.h
//!
//! @brief Internal API definition for Atomiq110 SDHC helpers.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************
//! @cond SDHC_PRIVATE_FUNC
#ifndef AM_HAL_SDHC_PRIVATE_H
#define AM_HAL_SDHC_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_hal_sdhc.h"

//
//! SDHC State structure.
//
typedef struct
{
    //
    //! Handle validation prefix.
    //
    am_hal_handle_prefix_t prefix;

    //
    //! Physical module number.
    //
    uint32_t ui32Module;
    uint32_t ui32HostSDMABufSize;
    uint8_t  ui8BaseClockFreq;

    //
    //! Link to the card host
    //
    am_hal_card_host_t *pHost;

    //
    //! Save the error count
    //
    bool bCmdErr;
    bool bDataErr;
    uint32_t ui32DataErrCnt;
    uint32_t ui32CmdErrCnt;

    //
    //! Store the data transfer information
    //
    uint32_t *pui32Buf;
    uint32_t ui32DataLen;
    uint32_t ui32BlkCnt;
    uint32_t ui32BlkNum;
    uint32_t ui32BlkSize;
    uint32_t ui32BlksPerSDMA;
    uint32_t ui32ClkOffDelay;
    am_hal_data_dir_e eDataDir;
    bool bAsyncCmdIsDone;

    //
    //! Store the DMA Buffer
    //
    void *pAdmaDescTable;

    //
    //! Power Save-Restore register state
    //
    am_hal_sdhc_register_state_t registerState;
} am_hal_sdhc_state_t;

typedef struct
{
    uint8_t  ui8Attr;
    uint8_t  ui8Reserved;
    uint16_t ui16Len;
    uint32_t ui32AddrLow;
} am_hal_sdhc_adma_desc_t;

// #### INTERNAL BEGIN ####
#ifndef BOOTROM_USE
// #### INTERNAL END ####
//*****************************************************************************
//
// External variable definitions
//
//*****************************************************************************
extern am_hal_card_host_t *g_CardHosts[AM_HAL_CARD_HOST_NUM];
extern am_hal_sdhc_state_t g_SDHCState[AM_HAL_CARD_HOST_NUM];
// #### INTERNAL BEGIN ####
#endif //BOOTROM_USE
// #### INTERNAL END ####

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SDHC_PRIVATE_H
//! @endcond AM_HAL_SDHC_PRIVATE_H
