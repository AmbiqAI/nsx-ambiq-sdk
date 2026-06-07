//*****************************************************************************
//
//! @file am_devices_spipsram.h
//!
//! @brief Generic SPI PSRAM driver.
//!
//! @addtogroup devices_spipsram Generic SPI PSRAM Driver
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

#ifndef AM_DEVICES_SPIPSRAM_H
#define AM_DEVICES_SPIPSRAM_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name  Global definitions for psram commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_SPIPSRAM_WRITE             0x02
#define AM_DEVICES_SPIPSRAM_READ              0x03
#define AM_DEVICES_SPIPSRAM_FAST_READ         0x0B
#define AM_DEVICES_SPIPSRAM_QUAD_MODE_ENTER   0x35
#define AM_DEVICES_SPIPSRAM_QUAD_WRITE        0x38
#define AM_DEVICES_SPIPSRAM_RESET_ENABLE      0x66
#define AM_DEVICES_SPIPSRAM_RESET_MEMORY      0x99
#define AM_DEVICES_SPIPSRAM_READ_ID           0x9F
#define AM_DEVICES_SPIPSRAM_HALF_SLEEP_ENTER  0xC0
#define AM_DEVICES_SPIPSRAM_QUAD_READ         0xEB
#define AM_DEVICES_SPIPSRAM_QUAD_MODE_EXIT    0xF5
//! @}

//*****************************************************************************
//
//! @name Device specific identification.
//! @{
//
//*****************************************************************************
#define AM_DEVICES_SPIPSRAM_KGD_PASS          0x5D0D
#define AM_DEVICES_SPIPSRAM_KGD_FAIL          0x550D

// Page size - limits the burst write/read
#define AM_DEVICES_SPIPSRAM_PAGE_SIZE         1024

//! @}

extern uint32_t g_APS6404LCS;

//*****************************************************************************
//
//! @name According to APS6404L tCEM restriction, we define maximum bytes for each speed empirically
//! @{
//
//*****************************************************************************
#define AM_DEVICES_SPIPSRAM_48MHZ_MAX_BYTES   32
#define AM_DEVICES_SPIPSRAM_24MHZ_MAX_BYTES   16
#define AM_DEVICES_SPIPSRAM_16MHZ_MAX_BYTES   10
#define AM_DEVICES_SPIPSRAM_12MHZ_MAX_BYTES   6
#define AM_DEVICES_SPIPSRAM_8MHZ_MAX_BYTES    3
//! @}

#define AM_DEVICES_APS6404L_MAX_DEVICE_NUM    8

//*****************************************************************************
//
//! @name  Global type definitions.
//! @{
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_SPIPSRAM_STATUS_SUCCESS,
    AM_DEVICES_SPIPSRAM_STATUS_ERROR
} am_devices_spipsram_status_t;

typedef struct
{
    uint32_t ui32ClockFreq;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
    uint32_t ui32ChipSelectNum ;
} am_devices_spipsram_config_t;
//! @}

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Initialize the spipsram driver.
//!
//! This function should be called before any other `am_devices_spipsram`
//! functions. It configures internal state to tell the other functions how
//! to communicate with the external SPI PSRAM hardware.
//!
//! @param ui32Module     - IOM Module#
//! @param pDevConfig     - Pointer to device configuration (`am_devices_spipsram_config_t`)
//! @param ppHandle       - Pointer to variable that will receive the device handle
//! @param ppIomHandle    - Pointer to variable that will receive the IOM handle
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_init(uint32_t ui32Module,
                                         am_devices_spipsram_config_t *pDevConfig,
                                         void **ppHandle,
                                         void **ppIomHandle);

//*****************************************************************************
//
//! @brief Initialize the spipsram driver without performing device checks.
//!
//! This function performs the same initialization as
//! `am_devices_spipsram_init` but does not attempt to verify the presence
//! or identity of the external PSRAM device. Use this when device probing
//! or ID verification is not required.
//!
//! @param ui32Module     - IOM module number to use for the device
//! @param pDevConfig     - Pointer to an `am_devices_spipsram_config_t`
//!                         structure containing device configuration
//! @param ppHandle       - Pointer to a variable that will receive the
//!                         device handle on success
//! @param ppIomHandle    - Pointer to a variable that will receive the
//!                         underlying IOM handle on success
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_init_no_check(uint32_t ui32Module,
                                                  am_devices_spipsram_config_t *pDevConfig,
                                                  void **ppHandle,
                                                  void **ppIomHandle);

//*****************************************************************************
//
//! @brief DeInitialize the spipsram driver.
//!
//! @param pHandle     - Pointer to device handle
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_term(void *pHandle);

//*****************************************************************************
//
//! @brief Reads the ID of the external psram and returns the value.
//!
//! This function reads the device ID register of the external PSRAM, and
//! returns the result as a 32-bit unsigned integer value in `pDeviceID`.
//!
//! @param pHandle   - Pointer to driver handle
//! @param pDeviceID - Pointer to the return buffer for the Device ID.
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_read_id(void *pHandle, uint32_t *pDeviceID);

//*****************************************************************************
//
//! @brief Reset the external PSRAM device.
//!
//! Issues the required reset enable and reset memory commands to the
//! external PSRAM device via the configured IOM interface. This triggers a
//! device reset and returns when the commands have been submitted.
//!
//! @param pHandle        - Pointer to driver handle returned from init
//!
//! @return 32-bit status (AM_DEVICES_SPIPSRAM_STATUS_SUCCESS on success)
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Reads the contents of the external psram into a buffer.
//!
//! This function reads the external PSRAM at the provided address and stores
//! the received data into the provided buffer location. It will read
//! exactly `ui32NumBytes` bytes starting at `ui32ReadAddress`.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8RxBuffer     - Buffer to store the received data from the psram
//! @param ui32ReadAddress  - Address of desired data in external psram
//! @param ui32NumBytes     - Number of bytes to read from external psram
//! @param bWaitForCompletion - If true, block until transfer completes
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_read(void *pHandle,
                                         uint8_t *pui8RxBuffer,
                                           uint32_t ui32ReadAddress,
                                           uint32_t ui32NumBytes,
                                           bool bWaitForCompletion);

//*****************************************************************************
//
//! @brief Reads the contents of the external psram into a buffer.
//!
//! This function reads the external PSRAM at the provided address and stores
//! the received data into the provided buffer location. It will read
//! exactly `ui32NumBytes` bytes starting at `ui32ReadAddress`. The transfer
//! may be controlled by `ui32PauseCondition`/`ui32StatusSetClr` and completion
//! can be reported via `pfnCallback`.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8RxBuffer     - Buffer to store the received data from the psram
//! @param ui32ReadAddress  - Address of desired data in external psram
//! @param ui32NumBytes     - Number of bytes to read from external psram
//! @param ui32PauseCondition - IOM pause condition flags (see `am_hal_iom`)
//! @param ui32StatusSetClr   - IOM status set/clear flags used for the transfer
//! @param pfnCallback        - Optional callback invoked when transfer completes
//! @param pCallbackCtxt      - Context pointer passed to `pfnCallback`
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_read_adv(void *pHandle,
                                           uint8_t *pui8RxBuffer,
                                           uint32_t ui32ReadAddress,
                                           uint32_t ui32NumBytes,
                                           uint32_t ui32PauseCondition,
                                           uint32_t ui32StatusSetClr,
                                           am_hal_iom_callback_t pfnCallback,
                                           void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Programs the given range of PSRAM addresses.
//!
//! This function copies data from `pui8TxBuffer` into external PSRAM at the
//! address `ui32WriteAddress`. It will write exactly `ui32NumBytes` bytes.
//! The caller must ensure the transfer does not overflow the PSRAM range and
//! that `pui8TxBuffer` contains sufficient data.
//!
//! @param pHandle            - Pointer to driver handle
//! @param pui8TxBuffer       - Buffer to write the external psram data from
//! @param ui32WriteAddress   - Address to write to in the external psram
//! @param ui32NumBytes       - Number of bytes to write to the external psram
//! @param bWaitForCompletion - If true, block until transfer completes
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_write(void *pHandle,  // dox check XXX
                                            uint8_t *pui8TxBuffer,
                                            uint32_t ui32WriteAddress,
                                            uint32_t ui32NumBytes,
                                            bool bWaitForCompletion);

//*****************************************************************************
//
//! @brief Advanced (asynchronous/controlled) write to the external PSRAM.
//!
//! Starts a write transaction that can be controlled via pause conditions
//! and status set/clear values and may complete asynchronously. When the
//! transaction completes (or encounters an error), the provided callback
//! will be invoked if non-NULL.
//!
//! @param pHandle            - Pointer to driver handle
//! @param puiTxBuffer        - Buffer containing the data to write
//! @param ui32WriteAddress   - Address in external PSRAM to begin writing
//! @param ui32NumBytes       - Number of bytes to write
//! @param ui32PauseCondition - IOM pause condition flags for the transfer
//! @param ui32StatusSetClr   - IOM status set/clear flags for the transfer
//! @param pfnCallback        - Optional callback invoked on completion
//! @param pCallbackCtxt      - Context pointer passed to the callback
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_write_adv(void *pHandle,
                                               uint8_t *puiTxBuffer,
                                               uint32_t ui32WriteAddress,
                                               uint32_t ui32NumBytes,
                                               uint32_t ui32PauseCondition,
                                               uint32_t ui32StatusSetClr,
                                               am_hal_iom_callback_t pfnCallback,
                                               void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Reads the contents of the external PSRAM into a buffer.
//!
//! This function reads the external PSRAM at the provided address and stores
//! the received data into the provided buffer location. It will read
//! exactly `ui32NumBytes` bytes starting at `ui32ReadAddress`. Completion
//! will be reported via `pfnCallback` when provided.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8RxBuffer     - Buffer to store the received data from the psram
//! @param ui32ReadAddress  - Address of desired data in external psram
//! @param ui32NumBytes     - Number of bytes to read from external psram
//! @param pfnCallback      - Callback invoked when transfer completes
//! @param pCallbackCtxt    - Context pointer passed to `pfnCallback`
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_nonblocking_read(void *pHandle,
                                                       uint8_t *pui8RxBuffer,
                                                       uint32_t ui32ReadAddress,
                                                       uint32_t ui32NumBytes,
                                                       am_hal_iom_callback_t pfnCallback,
                                                       void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Initiate a non-blocking (asynchronous) write to the external PSRAM.
//!
//! Begins a write operation and returns immediately. Completion (or error)
//! will be reported via the provided `pfnCallback` function. The caller must
//! ensure `pui8TxBuffer` remains valid until the callback is invoked.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8TxBuffer     - Buffer containing data to write
//! @param ui32WriteAddress - Address in external PSRAM to begin writing
//! @param ui32NumBytes     - Number of bytes to write
//! @param pfnCallback      - Callback invoked when transfer completes
//! @param pCallbackCtxt    - Context pointer passed to the callback
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_nonblocking_write(void *pHandle,
                                                      uint8_t *pui8TxBuffer,
                                                      uint32_t ui32WriteAddress,
                                                      uint32_t ui32NumBytes,
                                                      am_hal_iom_callback_t pfnCallback,
                                                      void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Programs the given range of PSRAM addresses.
//!
//! This function copies data from `pui8TxBuffer` into external PSRAM at the
//! address `ui32WriteAddress`. It will write exactly `ui32NumBytes` bytes.
//! The caller must ensure the transfer does not overflow the PSRAM range and
//! that `pui8TxBuffer` contains sufficient data.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8TxBuffer     - Buffer to write the external psram data from
//! @param ui32WriteAddress - Address to write to in the external psram
//! @param ui32NumBytes     - Number of bytes to write to the external psram
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_blocking_write(void *pHandle,
                                                   uint8_t *pui8TxBuffer,
                                                   uint32_t ui32WriteAddress,
                                                   uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Reads the contents of the PSRAM into a buffer.
//!
//! This function reads the external PSRAM at the provided address and stores
//! the received data into the provided buffer location. It will read
//! exactly `ui32NumBytes` bytes starting at `ui32ReadAddress`.
//!
//! @param pHandle          - Pointer to driver handle
//! @param pui8RxBuffer     - Buffer to store the received data from the psram
//! @param ui32ReadAddress  - Address of desired data in external psram
//! @param ui32NumBytes     - Number of bytes to read from external psram
//!
//! @return 32-bit status: `AM_DEVICES_SPIPSRAM_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SPIPSRAM_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_blocking_read(void *pHandle,
                                                  uint8_t *pui8RxBuffer,
                                                  uint32_t ui32ReadAddress,
                                                  uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Generic command write helper.
//!
//! Sends a command/instruction (of length `ui32InstrLen`) followed by
//! `ui32NumBytes` of data from `pData` to the PSRAM device. The operation
//! can be flagged as high-priority and may optionally keep chip select
//! asserted after the transfer if `bContinue` is true.
//!
//! @param pHandle        - Pointer to driver handle
//! @param bHiPrio        - Make this transfer high priority when true
//! @param ui32InstrLen   - Number of instruction/address bytes to send
//! @param ui64Instr      - Instruction/address bytes to transmit
//! @param pData          - Pointer to the data to transmit
//! @param ui32NumBytes   - Number of data bytes to transmit
//! @param bContinue      - If true, keep chip select asserted after transfer
//!
//! @return 32-bit status (AM_DEVICES_SPIPSRAM_STATUS_SUCCESS on success)
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_command_write(void *pHandle,
                          bool bHiPrio,
                          uint32_t ui32InstrLen,
                          uint64_t ui64Instr,
                          uint32_t *pData,
                          uint32_t ui32NumBytes,
                          bool bContinue);

//*****************************************************************************
//
//! @brief Generic command read helper.
//!
//! Sends a command/instruction (of length `ui32InstrLen`) then reads
//! `ui32NumBytes` of data from the PSRAM device into `pData`. The operation
//! can be flagged as high-priority and may optionally keep chip select
//! asserted after the transfer if `bContinue` is true.
//!
//! @param pHandle        - Pointer to driver handle
//! @param bHiPrio        - Make this transfer high priority when true
//! @param ui32InstrLen   - Number of instruction/address bytes to send
//! @param ui64Instr      - Instruction/address bytes to transmit
//! @param pData          - Pointer to buffer to receive the read data
//! @param ui32NumBytes   - Number of bytes to read into `pData`
//! @param bContinue      - If true, keep chip select asserted after transfer
//!
//! @return 32-bit status (AM_DEVICES_SPIPSRAM_STATUS_SUCCESS on success)
//
//*****************************************************************************
extern uint32_t am_devices_spipsram_command_read(
                        void *pHandle,
                        bool bHiPrio,
                        uint32_t ui32InstrLen,
                        uint64_t ui64Instr,
                        uint32_t *pData,
                        uint32_t ui32NumBytes,
                        bool bContinue);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_SPIPSRAM_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

