//*****************************************************************************
//
//! @file am_devices_xspi.h
//!
//! @brief xSPI device API definitions
//!
//! @addtogroup xspi_device_api xSPI device driver API
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

#ifndef AM_DEVICES_XSPI
#define AM_DEVICES_XSPI

#include <stdarg.h>
#include "am_hal_xspimc.h"

#ifdef __cplusplus
extern "C"
{
#endif

//! @defgroup devices_xspi_config xSPI device compile-time configuration
//! @ingroup xspi_device_api
//! @{
#ifndef AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE
#define AM_DEVICES_XSPI_PSRAM_AUTO_POWER_ENABLE 0
#endif
//! @}

typedef am_hal_periph_handle_t am_device_xspi_periph_handle_t;

//*****************************************************************************
//
//! @brief Common peripheral Helper Macro.
//! @defgroup xspi_periph_macros
//! @ingroup xspi_device_api
//! @{
//
//*****************************************************************************
#define AM_DEVICE_HANDLE_NAME(MODULE_NAME, DEV_NUM) AM_HAL_HANDLE_NAME(MODULE_NAME, DEV_NUM)                                                                                \

#define AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, SIZE, API, XSPI_HANDLE) \
        ATTR const void *EXPAND4(s, MODULE_NAME, CE_NUM, Deps)[] = { &(XSPI_HANDLE) };      \
        AM_HAL_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, SIZE, &(API),                       \
                             EXPAND4(s, MODULE_NAME, CE_NUM, Deps))

#define AM_DEVICE_XSPI_CHK_HANDLE(pHandle) AM_HAL_PERIPH_CHK_HANDLE(pHandle)

//! @} END_GROUP - Common peripheral Helper Macro.

//*****************************************************************************
//
//! @name Device specific identification.
//! @{
//
//*****************************************************************************

//! @}

//*****************************************************************************
//
//! @name Global definitions for the MSPI instance to use.
//! @{
//
//*****************************************************************************

//! @}
//*****************************************************************************
//
//! @name Global type definitions.
//! @{
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_XSPI_STATUS_SUCCESS,
    AM_DEVICES_XSPI_STATUS_ERROR
} am_devices_xspi_status_t;

typedef enum
{
    AM_DEVICES_XSPI_TYPE_PSRAM = 0,
    AM_DEVICES_XSPI_TYPE_NOR_FLASH,
    AM_DEVICES_XSPI_TYPE_NAND_FLASH,
    AM_DEVICES_XSPI_TYPE_BRIDGE,
} am_devices_xspi_type_e;

typedef enum
{
    AM_DEVICES_XSPI_POWER_ON = 0,
    AM_DEVICES_XSPI_POWER_OFF,
    AM_DEVICES_XSPI_POWER_AUTO,
} am_devices_xspi_power_e;

typedef struct
{
    void *pXSPI;
} am_devices_xspi_deps_t;

typedef struct
{
    uint32_t                   ui32BaseAddr;              // Aperture address where the AIP mapping starts.
    uint32_t                   ui32MappedDevBaseOffset;   // Device address presented at ui32BaseAddr.
    uint32_t                   ui32Size;                  // Size of the mapped span in bytes.
    bool                       bEnable;                   // Enable or disable the selected AIP window.
    uint8_t                    ui8WindowNum;
    am_hal_xspimc_aip_permit_e ePermission;
    bool                       bPrefetch;
    uint32_t                   ui32EncryptBaseAddr;
    uint32_t                   ui32EncryptSize;
    uint32_t                   ui32EncryptKey;
} am_devices_xspi_memmap_cfg_t;

typedef struct
{
    am_hal_xspimc_dev_cfg_t      sDevCfg;
    am_hal_xspimc_aip_cfg_t      sAIPCfg;
    am_hal_xspimc_timing_cfg_t   sTimingCfg;
} am_devices_xspi_hal_config_t;

typedef struct
{
    am_hal_xspimc_ce_t           sCE;
    uint32_t                     ui32Freq;
    am_hal_xspimc_io_mode_e      eIOMode;
    am_hal_xspimc_data_rate_e    eDataRate;
    bool                         bDQS;
    am_devices_xspi_memmap_cfg_t sMemMap;
} am_devices_xspi_psram_cfg_t;

typedef struct
{
    am_hal_xspimc_ce_t           sCE;
    uint32_t                     ui32Freq;
    am_hal_xspimc_io_mode_e      eIOMode;
    am_hal_xspimc_data_rate_e    eDataRate;
    bool                         bDQS;
    am_devices_xspi_memmap_cfg_t sMemMap;
    bool                         bEnableReadWhileWrite;
} am_devices_xspi_norflash_cfg_t;

typedef struct
{
    am_hal_xspimc_ce_t           sCE;
    uint32_t                     ui32Freq;
    am_hal_xspimc_io_mode_e      eIOMode;
    am_hal_xspimc_data_rate_e    eDataRate;
    bool                         bDQS;
    am_devices_xspi_memmap_cfg_t sMemMap;
} am_devices_xspi_nandflash_cfg_t;

typedef struct
{
    uint32_t ui32Freq;
    am_hal_xspimc_io_mode_e eIOMode;
    am_hal_xspimc_cpp_mode_e eCpp;
    uint32_t ui32ReadCmd;
    uint32_t ui32WriteCmd;
    uint8_t ui8RxDummy;
    uint8_t ui8TxDummy;
    am_hal_xspimc_ce_timing sCETiming;
} am_devices_xspi_bridge_cfg_t;

typedef void am_devices_xspi_config_t;

typedef am_hal_xspimc_timing_cfg_t am_devices_xspi_timing_t;

typedef struct
{
    uint8_t            ui8VendorId;
    uint8_t            ui8DeviceId;
    uint32_t           ui32BaseAddr;
    uint32_t           ui32DeviceSizeKb;
    am_hal_xspimc_ce_t sCE;
} am_devices_xspi_info_t;

typedef struct
{
    uint32_t (*device_init)            (am_device_xspi_periph_handle_t *pHandle);
    uint32_t (*device_deinit)          (am_device_xspi_periph_handle_t *pHandle);
    uint32_t (*device_config)          (am_device_xspi_periph_handle_t *pHandle,
                                        const am_devices_xspi_config_t *pConfig,
                                        ...);
    uint32_t (*device_get_info)        (am_device_xspi_periph_handle_t *pHandle,
                                        am_devices_xspi_info_t *pInfo);
    uint32_t (*device_write)           (am_device_xspi_periph_handle_t *pHandle,
                                        uint32_t ui32Addr,
                                        uint32_t ui32Size,
                                        void *pData);
    uint32_t (*device_read)            (am_device_xspi_periph_handle_t *pHandle,
                                        uint32_t ui32Addr,
                                        uint32_t ui32Size,
                                        void *pData);
    uint32_t (*device_erase)           (am_device_xspi_periph_handle_t *pHandle,
                                        uint32_t ui32Addr,
                                        uint32_t ui32Size);
    uint32_t (*device_set_xfer_mode)   (am_device_xspi_periph_handle_t *pHandle,
                                        am_hal_xspimc_xfer_mode_t eXferMode);
    uint32_t (*device_power_ctrl)      (am_device_xspi_periph_handle_t *pHandle,
                                        am_devices_xspi_power_e ePowerMode,
                                        ...);
    uint32_t (*device_mem_map_config)  (am_device_xspi_periph_handle_t *pHandle,
                                        const am_devices_xspi_memmap_cfg_t *pCfg);
    //uint32_t (*device_timing_scan)     (am_device_xspi_periph_handle_t *pHandle, am_devices_xspi_timing_scan_cfg_t pScanCfg, am_devices_xspi_timing_t *pResult);
    uint32_t (*device_timing_apply)    (am_device_xspi_periph_handle_t *pHandle,
                                        const am_devices_xspi_timing_t *pResult);
} am_device_xspi_func_t;
//! @}


//*****************************************************************************
//
//! @brief The generic xSPI device initialization.
//!
//! @param periph      - the handle for the module instance.
//!
//! This function accepts a handle to an instance and enable the
//! interface with a timeout.
//!
//! @return status     - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_device_xspi_bus_init(am_hal_periph_handle_t *pController,
                                        const am_hal_xspimc_cfg_t *pCfg);
extern uint32_t am_device_xspi_bus_deinit(am_hal_periph_handle_t *pController);

extern void am_device_xspi_cpu_vote(uint32_t ui32Xspi, bool bOn);

static inline uint32_t am_device_xspi_dev_init(am_device_xspi_periph_handle_t *pHandle)
{
    if (!pHandle || pHandle->state.bInitialized)
    {
        return AM_HAL_STATUS_INVALID_HANDLE;
    }

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_init)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_init(pHandle);
}

static inline uint32_t am_device_xspi_dev_deinit(am_device_xspi_periph_handle_t *pHandle)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_deinit)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_deinit(pHandle);
}

static inline uint32_t am_device_xspi_device_config(am_device_xspi_periph_handle_t *pHandle,
                                                    const am_devices_xspi_config_t *pConfig, ...)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_config)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    va_list ap;
    va_start(ap, pConfig);
    ui32Status = pApi->device_config(pHandle, pConfig, ap);
    va_end(ap);
    if (ui32Status)
    {
        return ui32Status;
    }

    return ui32Status;
}

static inline uint32_t am_device_xspi_device_read(am_device_xspi_periph_handle_t *pHandle,
                                                  uint32_t ui32Addr, uint32_t ui32Size, void *pData)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_read)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_read(pHandle, ui32Addr, ui32Size, pData);
}

static inline uint32_t am_device_xspi_device_write(am_device_xspi_periph_handle_t *pHandle,
                                                   uint32_t ui32Addr, uint32_t ui32Size,
                                                   void *pData)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_write)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_write(pHandle, ui32Addr, ui32Size, pData);
}

static inline uint32_t am_device_xspi_device_erase(am_device_xspi_periph_handle_t *pHandle,
                                                   uint32_t ui32Addr, uint32_t ui32Size)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_erase)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_erase(pHandle, ui32Addr, ui32Size);
}

static inline uint32_t am_device_xspi_device_set_xfer_mode(am_device_xspi_periph_handle_t *pHandle,
                                                           am_hal_xspimc_xfer_mode_t eXferMode)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_set_xfer_mode)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_set_xfer_mode(pHandle, eXferMode);
}

static inline uint32_t am_device_xspi_device_get_info(am_device_xspi_periph_handle_t *pHandle,
                                                      am_devices_xspi_info_t *pInfo)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_get_info)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_get_info(pHandle, pInfo);
}

static inline uint32_t am_device_xspi_mem_map_config(am_device_xspi_periph_handle_t *pHandle,
                                                     const am_devices_xspi_memmap_cfg_t *pCfg)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_mem_map_config)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_mem_map_config(pHandle, pCfg);
}

static inline uint32_t am_device_xspi_power_control(am_device_xspi_periph_handle_t *pHandle,
                                                    am_devices_xspi_power_e ePowerMode, ...)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_power_ctrl)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    uint32_t ui32Status = AM_DEVICES_XSPI_STATUS_SUCCESS;

    va_list ap;
    va_start(ap, ePowerMode);
    ui32Status = pApi->device_power_ctrl(pHandle, ePowerMode, ap);
    va_end(ap);
    if (ui32Status)
    {
        return ui32Status;
    }

    return ui32Status;
}

static inline uint32_t am_device_xspi_timing_apply(am_device_xspi_periph_handle_t *pHandle,
                                                   const am_devices_xspi_timing_t *pTiming)
{
    AM_DEVICE_XSPI_CHK_HANDLE(pHandle)

    const am_device_xspi_func_t *pApi = (const am_device_xspi_func_t *)pHandle->pApi;
    if (!pApi || !pApi->device_timing_apply)
    {
        return AM_HAL_STATUS_INVALID_OPERATION;
    }

    return pApi->device_timing_apply(pHandle, pTiming);
}

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
