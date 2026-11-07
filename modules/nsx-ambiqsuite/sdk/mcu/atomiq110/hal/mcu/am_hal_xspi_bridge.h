//*****************************************************************************
//
//! @file am_hal_xspi_bridge.h
//!
//! @brief Functions for operating the xSPI bridge.
//!
//! @addtogroup xspi_bridge_at110 XSPI Bridge - Subordinate Side
//! @ingroup atomiq110_hal
//! @{
//
//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_XSPI_BRIDGE_H
#define AM_HAL_XSPI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! CMSIS-style accessors for the xSPI bridge register blocks.
//
//*****************************************************************************
#define XSPISn(n)   ((XSPIS_Type *)(AM_REG_XSPISn(n)))
#define XSPISLVn(n) ((XSPISLV_Type *)(AM_REG_XSPISLVn(n)))

//*****************************************************************************
//
//! @name xSPI Bridge limits and status bits
//! @{
//
//*****************************************************************************
#define AM_HAL_XSPI_BRIDGE_MAX_MODULES           1
#define AM_HAL_XSPI_BRIDGE_MAX_ADDR_ZONES        4

#define AM_HAL_XSPI_BRIDGE_STS_SYNC_Pos          24
#define AM_HAL_XSPI_BRIDGE_STS_SYNC_Msk          (0xFFu << AM_HAL_XSPI_BRIDGE_STS_SYNC_Pos)
#define AM_HAL_XSPI_BRIDGE_STS_DUMMY_Pos         16
#define AM_HAL_XSPI_BRIDGE_STS_DUMMY_Msk         (0xFFu << AM_HAL_XSPI_BRIDGE_STS_DUMMY_Pos)
// #### INTERNAL BEGIN ####
#define AM_HAL_XSPI_BRIDGE_STS_DQSEN_Pos         15
#define AM_HAL_XSPI_BRIDGE_STS_DQSEN_Msk         (0x1u << AM_HAL_XSPI_BRIDGE_STS_DQSEN_Pos)
// #### INTERNAL END ####
#define AM_HAL_XSPI_BRIDGE_STS_CMODE_Pos         12
#define AM_HAL_XSPI_BRIDGE_STS_CMODE_Msk         (0x3u << AM_HAL_XSPI_BRIDGE_STS_CMODE_Pos)
#define AM_HAL_XSPI_BRIDGE_STS_AMODE_Pos         10
#define AM_HAL_XSPI_BRIDGE_STS_AMODE_Msk         (0x3u << AM_HAL_XSPI_BRIDGE_STS_AMODE_Pos)
#define AM_HAL_XSPI_BRIDGE_STS_DMODE_Pos         8
#define AM_HAL_XSPI_BRIDGE_STS_DMODE_Msk         (0x3u << AM_HAL_XSPI_BRIDGE_STS_DMODE_Pos)
#define AM_HAL_XSPI_BRIDGE_STS_BURSTERR_Msk      (0x1u << 7)
#define AM_HAL_XSPI_BRIDGE_STS_OVERRUN_Msk       (0x1u << 6)
#define AM_HAL_XSPI_BRIDGE_STS_UNDERRUN_Msk      (0x1u << 5)
#define AM_HAL_XSPI_BRIDGE_STS_BUSERR_Msk        (0x1u << 4)
#define AM_HAL_XSPI_BRIDGE_STS_INVLEN_Msk        (0x1u << 3)
#define AM_HAL_XSPI_BRIDGE_STS_INVCMD_Msk        (0x1u << 2)
#define AM_HAL_XSPI_BRIDGE_STS_WRITE_Msk         (0x1u << 1)
#define AM_HAL_XSPI_BRIDGE_STS_BUSY_Msk          (0x1u << 0)

#define AM_HAL_XSPI_BRIDGE_STS_STICKY_Msk        (AM_HAL_XSPI_BRIDGE_STS_BURSTERR_Msk  | \
                                                  AM_HAL_XSPI_BRIDGE_STS_OVERRUN_Msk   | \
                                                  AM_HAL_XSPI_BRIDGE_STS_UNDERRUN_Msk  | \
                                                  AM_HAL_XSPI_BRIDGE_STS_BUSERR_Msk    | \
                                                  AM_HAL_XSPI_BRIDGE_STS_INVLEN_Msk    | \
                                                  AM_HAL_XSPI_BRIDGE_STS_INVCMD_Msk)
//! @}

//*****************************************************************************
//
//! @name Bridge interrupt status
//! @{
//
//*****************************************************************************
#define AM_HAL_XSPI_BRIDGE_INT_ACCERR            XSPISLV_INTEN_ACCERR_Msk
#define AM_HAL_XSPI_BRIDGE_INT_OOBDET            XSPISLV_INTEN_OOBDET_Msk
#define AM_HAL_XSPI_BRIDGE_INT_ALL               (AM_HAL_XSPI_BRIDGE_INT_ACCERR | \
                                                  AM_HAL_XSPI_BRIDGE_INT_OOBDET)
//! @}

//*****************************************************************************
//
//! @name Address-filter error status
//! @{
//
//*****************************************************************************
#define AM_HAL_XSPI_BRIDGE_ERROR_OOB_DETECTED    XSPISLV_ERRSTS_OOBDETECTED_Msk
#define AM_HAL_XSPI_BRIDGE_ERROR_ACCESS_DETECTED XSPISLV_ERRSTS_ERRDETECTED_Msk
#define AM_HAL_XSPI_BRIDGE_ERROR_ALL              \
    (AM_HAL_XSPI_BRIDGE_ERROR_OOB_DETECTED |      \
     AM_HAL_XSPI_BRIDGE_ERROR_ACCESS_DETECTED)
//! @}

//*****************************************************************************
//
//! Transfer mode options (x1, x2, x4)
//
//*****************************************************************************
typedef enum
{
    AM_HAL_XSPI_BRIDGE_XMODE_1 = 0,
    AM_HAL_XSPI_BRIDGE_XMODE_2 = 1,
    AM_HAL_XSPI_BRIDGE_XMODE_4 = 2,
    // #### INTERNAL BEGIN ####
    AM_HAL_XSPI_BRIDGE_XMODE_8 = 3
    // #### INTERNAL END ####
} am_hal_xspi_bridge_xmode_e;

//*****************************************************************************
//
//! SPI modes supported by xSPI bridge.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_XSPI_BRIDGE_SPI_MODE_0 = 0,
    AM_HAL_XSPI_BRIDGE_SPI_MODE_1 = 1
} am_hal_xspi_bridge_spi_mode_e;

//*****************************************************************************
//
//! Parsed XSPIS.STS contents.
//
//*****************************************************************************
typedef struct
{
    uint8_t ui8SyncHeader;
    bool    bIsBusy;
    bool    bIsWrite;
    struct
    {
        bool bBurstErr;
        bool bOverrun;
        bool bUnderrun;
        bool bBusErr;
        bool bInvLen;
        bool bInvCmd;
    } sErrorFlags;
} am_hal_xspi_bridge_status_t;

//*****************************************************************************
//
//! Parsed XSPIS.STS configuration fields.
//
//*****************************************************************************
typedef struct
{
    am_hal_xspi_bridge_xmode_e eCMode;
    am_hal_xspi_bridge_xmode_e eAMode;
    am_hal_xspi_bridge_xmode_e eDMode;
    uint8_t                    ui8DummyCycles;
    // #### INTERNAL BEGIN ####
    bool                       bDQSEnable;
    // #### INTERNAL END ####
} am_hal_xspi_bridge_config_t;

//*****************************************************************************
//
//! Per-address-zone permission settings.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_XSPI_BRIDGE_ADDR_ZONE_PERM_NONE = 0x0,
    AM_HAL_XSPI_BRIDGE_ADDR_ZONE_PERM_RO   = 0x1,
    AM_HAL_XSPI_BRIDGE_ADDR_ZONE_PERM_WO   = 0x2,
    AM_HAL_XSPI_BRIDGE_ADDR_ZONE_PERM_RW   = 0x3
} am_hal_xspi_bridge_addr_zone_perm_e;

//*****************************************************************************
//
//! Individual address zone settings.
//
//*****************************************************************************
typedef struct
{
    bool                               bEnable;
    uint32_t                           ui32StartAddr;
    uint32_t                           ui32EndAddr;
    am_hal_xspi_bridge_addr_zone_perm_e ePermissions;
} am_hal_xspi_bridge_addr_zone_t;

//*****************************************************************************
//
//! Address zone filter configuration for xSPI bridge.
//
//*****************************************************************************
typedef struct
{
    bool                         bGlobalFilterEnable;
    bool                         bOobErrorMask;
    am_hal_xspi_bridge_addr_zone_t sZones[AM_HAL_XSPI_BRIDGE_MAX_ADDR_ZONES];
} am_hal_xspi_bridge_addr_zone_cfg_t;

//*****************************************************************************
//
//! Address-filter error status captured by xSPI bridge.
//
//*****************************************************************************
typedef struct
{
    //! OR of the `AM_HAL_XSPI_BRIDGE_ERROR_*` status bits.
    uint32_t ui32ErrorStatus;
    //! First address captured for a masked out-of-bounds read.
    uint32_t ui32OobAddress;
    //! Address captured for an address-filter access violation.
    uint32_t ui32AccessErrorAddress;
    //! Valid when `AM_HAL_XSPI_BRIDGE_ERROR_ACCESS_DETECTED` is set.
    bool     bAccessErrorIsWrite;
} am_hal_xspi_bridge_error_status_t;

//*****************************************************************************
//
//! Control commands for `am_hal_xspi_bridge_control()`.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_XSPI_BRIDGE_CONTROL_ADDR_ZONE_CONFIG = 0,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_STATUS,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_CONFIG,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_LAST_ADDRESS,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_LAST_RX_WORD,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_LAST_TX_WORD,
    AM_HAL_XSPI_BRIDGE_CONTROL_SPI_MODE_CONFIG,
    AM_HAL_XSPI_BRIDGE_CONTROL_GET_ERROR_STATUS,
    AM_HAL_XSPI_BRIDGE_CONTROL_CLEAR_ERROR_STATUS
} am_hal_xspi_bridge_control_e;

//*****************************************************************************
//
//! @brief Callback invoked by am_hal_xspi_bridge_interrupt_service().
//!
//! @param ui32IntStatus - Mask of AM_HAL_XSPI_BRIDGE_INT_* events being serviced.
//! @param pCallbackCtxt - Context supplied when registering the callback.
//!
//! The callback runs in the context of the interrupt_service() caller.
//
//*****************************************************************************
typedef void (*am_hal_xspi_bridge_callback_t)(uint32_t ui32IntStatus, void *pCallbackCtxt);

//*****************************************************************************
//
//! External function definitions
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Initialize an xSPI bridge handle without powering on the peripheral.
//!
//! @param ui32Module - Bridge module number (0).
//! @param ppHandle - Receives the initialized handle.
//!
//! @return AM_HAL_STATUS_SUCCESS, AM_HAL_STATUS_OUT_OF_RANGE,
//!         AM_HAL_STATUS_INVALID_ARG, or AM_HAL_STATUS_INVALID_OPERATION.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_initialize(uint32_t ui32Module,
                                            void **ppHandle);

//*****************************************************************************
//
//! @brief Release an initialized xSPI bridge handle.
//!
//! @param pHandle - Handle returned by am_hal_xspi_bridge_initialize().
//!
//! Power down the bridge before calling this function. It does not disable
//! peripheral power, release clocks, or disable NVIC interrupts.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_uninitialize(void *pHandle);

//*****************************************************************************
//
//! @brief Power the bridge on or off, optionally retaining its configuration.
//!
//! @param pHandle - Handle returned by am_hal_xspi_bridge_initialize().
//! @param ePowerState - AM_HAL_SYSCTRL_WAKE to power on, or
//!                     AM_HAL_SYSCTRL_NORMALSLEEP/DEEPSLEEP to power off.
//! @param bRetainState - Save address zones, filter settings, clock settings,
//!                      and interrupt enables on power down; restore on wake.
//!
//! Waking with bRetainState set requires a configuration saved by a previous
//! power-down call. Stop host transfers before powering down the bridge.
//!
//! @return AM_HAL_STATUS_SUCCESS, AM_HAL_STATUS_INVALID_HANDLE,
//!         AM_HAL_STATUS_INVALID_ARG, AM_HAL_STATUS_INVALID_OPERATION,
//!         or an error returned by PWRCTRL or CLKMGR.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_power_control(void *pHandle,
                                               am_hal_sysctrl_power_state_e ePowerState,
                                               bool bRetainState);

//*****************************************************************************
//
//! @brief Enable selected bridge interrupt sources.
//!
//! @param pHandle - Initialized handle for a powered-on bridge.
//! @param ui32IntMask - OR of AM_HAL_XSPI_BRIDGE_INT_* bits to enable.
//!
//! Unsupported mask bits are ignored. NVIC configuration is the caller's
//! responsibility.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_enable(void *pHandle,
                                                  uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Disable selected bridge interrupt sources.
//!
//! @param pHandle - Initialized handle for a powered-on bridge.
//! @param ui32IntMask - OR of AM_HAL_XSPI_BRIDGE_INT_* bits to disable.
//!
//! Unsupported mask bits are ignored. This does not disable the NVIC IRQ.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_disable(void *pHandle,
                                                   uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Register or remove the bridge interrupt callback.
//!
//! @param pHandle - Handle returned by am_hal_xspi_bridge_initialize().
//! @param pfnCallback - Callback to register, or NULL to remove the callback.
//! @param pCallbackCtxt - Application context passed to the callback.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_register_callback(
    void *pHandle,
    am_hal_xspi_bridge_callback_t pfnCallback,
    void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Dispatch bridge interrupt events to the registered callback.
//!
//! @param pHandle - Handle returned by am_hal_xspi_bridge_initialize().
//! @param ui32IntStatus - OR of AM_HAL_XSPI_BRIDGE_INT_* events to service.
//!
//! The callback is invoked synchronously for supported nonzero status bits.
//! Read and clear pending interrupts separately; this function does not clear
//! hardware interrupt or address-filter error status.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_service(void *pHandle,
                                                   uint32_t ui32IntStatus);

//*****************************************************************************
//
//! @brief Read pending bridge interrupt status.
//!
//! @param pHandle - Initialized handle for a powered-on bridge.
//! @param bEnabledOnly - Return only enabled sources when true.
//! @param pui32IntStatus - Receives an OR of AM_HAL_XSPI_BRIDGE_INT_* bits.
//!
//! @return AM_HAL_STATUS_SUCCESS, AM_HAL_STATUS_INVALID_HANDLE,
//!         or AM_HAL_STATUS_INVALID_ARG.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_status_get(void *pHandle,
                                                      bool bEnabledOnly,
                                                      uint32_t *pui32IntStatus);

//*****************************************************************************
//
//! @brief Clear selected pending bridge interrupts.
//!
//! @param pHandle - Initialized handle for a powered-on bridge.
//! @param ui32IntMask - OR of AM_HAL_XSPI_BRIDGE_INT_* bits to clear.
//!
//! Unsupported mask bits are ignored. Address-filter error status is cleared
//! separately with AM_HAL_XSPI_BRIDGE_CONTROL_CLEAR_ERROR_STATUS.
//!
//! @return AM_HAL_STATUS_SUCCESS or AM_HAL_STATUS_INVALID_HANDLE.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_interrupt_clear(void *pHandle,
                                                 uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Configure the bridge or read its transaction and error status.
//!
//! @param pHandle - Initialized handle for a powered-on bridge.
//! @param eControl - Command from am_hal_xspi_bridge_control_e.
//! @param pArgs - Non-NULL input/output pointer of the type documented for
//!                eControl in am_hal_xspi_bridge_control_e.
//!
//! Stop host transfers before changing SPI mode or address-zone configuration.
//! Enabled zones require a 4 KB aligned start and exclusive end address, with
//! the inclusive end supplied in ui32EndAddr. Invalid zone addresses leave all
//! filter registers unchanged. Fields of disabled zones are ignored.
//!
//! @return AM_HAL_STATUS_SUCCESS, AM_HAL_STATUS_INVALID_HANDLE,
//!         or AM_HAL_STATUS_INVALID_ARG.
//
//*****************************************************************************
extern uint32_t am_hal_xspi_bridge_control(void *pHandle,
                                         am_hal_xspi_bridge_control_e eControl,
                                         void *pArgs);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_XSPI_BRIDGE_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
