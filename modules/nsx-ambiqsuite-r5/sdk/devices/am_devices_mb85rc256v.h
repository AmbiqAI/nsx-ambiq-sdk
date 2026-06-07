//*****************************************************************************
//
//! @file am_devices_mb85rc256v.h
//!
//! @brief Device driver for Fujitsu MB85RC256V peripheral.
//!
//! @addtogroup devices_mb85rc256v Fujitsu MB85RC256V Device Driver
//! @ingroup devices
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_DEVICES_MB85RC256V_H
#define AM_DEVICES_MB85RC256V_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Global definitions for the commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MB85RC256V_PERIPHERAL_ID          0x50
#define AM_DEVICES_MB85RC256V_RSVD_PERIPHERAL_ID     0x7C
#define AM_DEVICES_MB85RC256V_ID                     0x0010A500
#define AM_DEVICES_MB85RC64TA_ID                     0x0058A300
#define AM_DEVICES_MB85RC1MT_ID                      0x0058A700
//! @}

//*****************************************************************************
//
//! Global type definitions.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_MB85RC256V_STATUS_SUCCESS,
    AM_DEVICES_MB85RC256V_STATUS_ERROR
} am_devices_mb85rc256v_status_t;

typedef struct
{
    uint32_t ui32ClockFreq;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
} am_devices_mb85rc256v_config_t;

#define AM_DEVICES_MB85RC256V_MAX_DEVICE_NUM    1

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Initialize the mb85rc256v driver.
//!
//! This function initializes driver state and configures how the driver
//! communicates with the external FRAM device. Call this before any other
//! `am_devices_mb85rc256v` APIs.
//!
//! @param ui32Module      The IOM/MSPI module number used to communicate.
//! @param pDevConfig      Pointer to the device configuration structure.
//! @param ppHandle        Pointer to a variable that will receive the device
//!                        handle on successful initialization.
//! @param ppIomHandle     Pointer to a variable that will receive the IOM
//!                        handle associated with the device.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_init(uint32_t ui32Module,
                                           am_devices_mb85rc256v_config_t *pDevConfig,
                                           void **ppHandle,
                                           void **ppIomHandle);

//*****************************************************************************
//
//! @brief De-Initialize the mb85rc256v driver.
//!
//! This function reverses the initialization performed by
//! `am_devices_mb85rc256v_init` and frees any resources associated with the
//! supplied device handle.
//!
//! @param pHandle     Pointer to the device handle to terminate.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_term(void *pHandle);

//*****************************************************************************
//
//! @brief Read the device ID of the external FRAM.
//!
//! This function reads the device ID register of the external FRAM and
//! returns the value through the provided output pointer.
//!
//! @param pHandle    Pointer to the device handle.
//! @param pDeviceID  Pointer to a uint32_t that will receive the device ID.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_read_id(void *pHandle,
                                              uint32_t *pDeviceID);

//*****************************************************************************
//
//! @brief Write data to the FRAM (blocking).
//!
//! This function writes exactly `ui32NumBytes` from `pui8TxBuffer` to the
//! external FRAM starting at `ui32WriteAddress`. The call blocks until the
//! transfer completes.
//!
//! @param pHandle          Device handle for the external FRAM.
//! @param pui8TxBuffer     Pointer to the transmit buffer containing data to
//!                        be written.
//! @param ui32WriteAddress Start address in FRAM to write to.
//! @param ui32NumBytes     Number of bytes to write.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_blocking_write(void *pHandle,
                                                     uint8_t *pui8TxBuffer,
                                                     uint32_t ui32WriteAddress,
                                                     uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Write data to the FRAM (non-blocking).
//!
//! This function starts a non-blocking write of `ui32NumBytes` from
//! `pui8TxBuffer` to the external FRAM at `ui32WriteAddress`. The provided
//! callback will be invoked when the transfer completes.
//!
//! @param pHandle          Device handle for the external FRAM.
//! @param pui8TxBuffer     Pointer to the transmit buffer containing data to
//!                        be written.
//! @param ui32WriteAddress Start address in FRAM to write to.
//! @param ui32NumBytes     Number of bytes to write.
//! @param pfnCallback      Callback invoked when the write completes.
//! @param pCallbackCtxt    User context passed to the callback.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_nonblocking_write(void *pHandle,
                                                        uint8_t *pui8TxBuffer,
                                                        uint32_t ui32WriteAddress,
                                                        uint32_t ui32NumBytes,
                                                        am_hal_iom_callback_t pfnCallback,
                                                        void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Read data from the FRAM (blocking).
//!
//! This function reads `ui32NumBytes` from the external FRAM starting at
//! `ui32ReadAddress` into `pui8RxBuffer`. The call blocks until the read is
//! complete.
//!
//! @param pHandle          Device handle for the external FRAM.
//! @param pui8RxBuffer     Pointer to the buffer that will receive data.
//! @param ui32ReadAddress  Start address in FRAM to read from.
//! @param ui32NumBytes     Number of bytes to read.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_blocking_read(void *pHandle,
                                                    uint8_t *pui8RxBuffer,
                                                    uint32_t ui32ReadAddress,
                                                    uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Read data from the FRAM (non-blocking).
//!
//! This function starts a non-blocking read of `ui32NumBytes` from
//! `ui32ReadAddress` into `pui8RxBuffer`. The provided callback will be
//! invoked when the read completes.
//!
//! @param pHandle          Device handle for the external FRAM.
//! @param pui8RxBuffer     Pointer to the buffer that will receive data.
//! @param ui32ReadAddress  Start address in FRAM to read from.
//! @param ui32NumBytes     Number of bytes to read.
//! @param pfnCallback      Callback invoked when the read completes.
//! @param pCallbackCtxt    User context passed to the callback.
//!
//! @return `AM_DEVICES_MB85RC256V_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MB85RC256V_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mb85rc256v_nonblocking_read(void *pHandle,
                                                       uint8_t *pui8RxBuffer,
                                                       uint32_t ui32ReadAddress,
                                                       uint32_t ui32NumBytes,
                                                       am_hal_iom_callback_t pfnCallback,
                                                       void *pCallbackCtxt);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_MB85RC256V_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

