//*****************************************************************************
//
//! @file am_devices_mspi_rm67162.h
//!
//! @brief Multi-bit SPI Display driver for the RM67162 display panel.
//!
//! @addtogroup devices_mspi_rm67162 RM67162 MSPI Display Driver
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

#ifndef AM_DEVICES_RM67162_H
#define AM_DEVICES_RM67162_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Display Dimensions
//! @{
//
//*****************************************************************************
#define AM_DEVICES_RM67162_NUM_ROWS                      400
#define AM_DEVICES_RM67162_NUM_COLUMNS                   400
#define AM_DEVICES_RM67162_PIXEL_SIZE                    1

//! @}

//*****************************************************************************
//
//! @name Global definitions for the commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_RM67162_SWRESET                       0x01
#define AM_DEVICES_RM67162_READ_ID                       0x04
#define AM_DEVICES_RM67162_RD_PXL_FORMAT                 0x0C
#define AM_DEVICES_RM67162_SLEEP_IN                      0x10
#define AM_DEVICES_RM67162_SLEEP_OUT                     0x11
#define AM_DEVICES_RM67162_INVERSION_ON                  0x21
#define AM_DEVICES_RM67162_DISPLAY_OFF                   0x28
#define AM_DEVICES_RM67162_DISPLAY_ON                    0x29
#define AM_DEVICES_RM67162_COLUMN_ADDR_SETTING           0x2A
#define AM_DEVICES_RM67162_ROW_ADDR_SETTING              0x2B
#define AM_DEVICES_RM67162_MEMORY_WRITE                  0x2C
#define AM_DEVICES_RM67162_MEMORY_READ                   0x2E
#define AM_DEVICES_RM67162_TEARING_EFFECT_LINE_ON        0x35
#define AM_DEVICES_RM67162_SCAN_MODE                     0x36
#define AM_DEVICES_RM67162_HIGH_POWER_MODE_ON            0x38
#define AM_DEVICES_RM67162_LOW_POWER_MODE_ON             0x39
#define AM_DEVICES_RM67162_DATA_FORMAT_SEL               0x3A
#define AM_DEVICES_RM67162_MEMORY_WRITE_CONTINUE         0x3C
#define AM_DEVICES_RM67162_MEMORY_READ_CONTINUE          0x3E
#define AM_DEVICES_RM67162_SET_TEAR_SCANLINE             0x44
#define AM_DEVICES_RM67162_SET_WRITE_DISPLAY_CTRL        0x53
#define AM_DEVICES_RM67162_DUTY_SETTING                  0xB0
#define AM_DEVICES_RM67162_FRAME_RATE_CTRL               0xB2
#define AM_DEVICES_RM67162_UPDATE_PERIOD_GATE_EQ_CTRL    0xB4
#define AM_DEVICES_RM67162_DESTRESS_PERIOD_GATE_EQ_CTRL  0xB5
#define AM_DEVICES_RM67162_PANEL_SETTING                 0xB8
#define AM_DEVICES_RM67162_SOURCE_SETTING                0xB9
#define AM_DEVICES_RM67162_GATE_VOL_CTRL                 0xC0
#define AM_DEVICES_RM67162_SET_DSPI_MODE                 0xC4
#define AM_DEVICES_RM67162_OSC_ENABLE                    0xC7
#define AM_DEVICES_RM67162_VCOMH_VOL_CTRL                0xCB
#define AM_DEVICES_RM67162_BOOSTER_ENABLE                0xD1
#define AM_DEVICES_RM67162_4SPI_INPUT_DATA_SEL           0xE4
#define AM_DEVICES_RM67162_MTP_LOAD_CTRL                 0xEB
//! @}

//*****************************************************************************
//
//! @name Global type definitions.
//! @{
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_RM67162_STATUS_SUCCESS,
    AM_DEVICES_RM67162_STATUS_ERROR
} am_devices_rm67162_status_t;

//! @}

#define AM_DEVICES_RM67162_SPI_WRAM                     0x80
#define AM_DEVICES_RM67162_DSPI_WRAM                    0x81

#define AM_DEVICES_RM67162_COLOR_MODE_8BIT              0x72
#define AM_DEVICES_RM67162_COLOR_MODE_3BIT              0x71
#define AM_DEVICES_RM67162_COLOR_MODE_16BIT             0x75
#define AM_DEVICES_RM67162_COLOR_MODE_24BIT             0x77

#define AM_DEVICES_RM67162_SCAN_MODE_0                  0x40
#define AM_DEVICES_RM67162_SCAN_MODE_90                 0x70
#define AM_DEVICES_RM67162_SCAN_MODE_180                0x10
#define AM_DEVICES_RM67162_SCAN_MODE_270                0x00

typedef struct
{
    uint8_t bus_mode;
    uint8_t color_mode;
    uint8_t scan_mode;

    uint32_t max_row;
    uint32_t max_col;
    uint32_t row_offset;
    uint32_t col_offset;
} am_devices_rm67162_graphic_conf_t;

typedef struct
{
    am_hal_mspi_device_e eDeviceConfig;
    am_hal_mspi_clock_e eClockFreq;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
    uint32_t ui32ScramblingStartAddr;
    uint32_t ui32ScramblingEndAddr;
} am_devices_mspi_rm67162_config_t;

#define AM_DEVICES_MSPI_RM67162_MAX_DEVICE_NUM    1

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Reset the RM67162 display device.
//!
//! Issues the display reset sequence to bring the panel into a known state.
//!
//! @param pHandle - Pointer to driver handle.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Turn the display off (enter sleep or display-off state).
//!
//! @param pHandle - Pointer to driver handle.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_display_off(void *pHandle);

//*****************************************************************************
//
//! @brief Turn the display on (exit sleep / enable display output).
//!
//! @param pHandle - Pointer to driver handle.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_display_on(void *pHandle);

//*****************************************************************************
//
//! @brief Programs the given range of flash addresses.
//!
//! @param pHandle
//! @param pui8TxBuffer - Buffer to write the external flash data from
//! @param ui32NumBytes - Number of bytes to write to the external flash
//!
//! This function uses the data in the provided pui8TxBuffer and copies it to
//! the external flash at the address given by ui32WriteAddress. It will copy
//! exactly ui32NumBytes of data from the original pui8TxBuffer pointer. The
//! user is responsible for ensuring that they do not overflow the target flash
//! memory or underflow the pui8TxBuffer array
//!
//! @return 32-bit status
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_blocking_write(void *pHandle,
                                                uint8_t *pui8TxBuffer,
                                                uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Blocking read from the display memory.
//!
//! Reads `ui32NumBytes` of display memory into `pui8RxBuffer`. This call
//! blocks until the transfer completes.
//!
//! @param pHandle       - Pointer to driver handle.
//! @param pui8RxBuffer  - Buffer to receive the data.
//! @param ui32NumBytes  - Number of bytes to read.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_blocking_read(void *pHandle,
                                               uint8_t *pui8RxBuffer,
                                               uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Non-blocking write to the display memory.
//!
//! Queues a transfer of `ui32NumBytes` from `pui8TxBuffer` to the display.
//! If `bWaitForCompletion` is true the function will wait for completion.
//!
//! @param pHandle             - Pointer to driver handle.
//! @param pui8TxBuffer        - Buffer containing data to write.
//! @param ui32NumBytes        - Number of bytes to write.
//! @param bWaitForCompletion  - When true, block until transfer completes.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_write(void *pHandle,
                                                   uint8_t *pui8TxBuffer,
                                                   uint32_t ui32NumBytes,
                                                   bool bWaitForCompletion);

//*****************************************************************************
//
//! @brief Programs the given range of display addresses.
//!
//! @param pHandle       -
//! @param pui8TxBuffer - Buffer to write the data from
//! @param ui32NumBytes - Number of bytes to write to the display memory
//! @param ui32PauseCondition - CQ Pause condition before execution.
//! @param ui32StatusSetClr - CQ Set/Clear condition after execution.
//! @param pfnCallback - Callback function after execution.
//! @param pCallbackCtxt - Callback context after execution.
//!
//! This function uses the data in the provided pui8TxBuffer and copies it to
//! the external display the address given by ui32WriteAddress. It will copy
//! exactly ui32NumBytes of data from the original pui8TxBuffer pointer. The
//! user is responsible for ensuring that they do not overflow the target display
//! memory or underflow the pui8TxBuffer array
//!
//! @return 32-bit status
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_write_adv(void *pHandle,
                                                       uint8_t *pui8TxBuffer,
                                                       uint32_t ui32NumBytes,
                                                       uint32_t ui32PauseCondition,
                                                       uint32_t ui32StatusSetClr,
                                                       am_hal_mspi_callback_t pfnCallback,
                                                       void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Non-blocking read from the display memory.
//!
//! Initiates a read of `ui32NumBytes` into `pui8RxBuffer`. If
//! `bWaitForCompletion` is true this call blocks until the transfer completes.
//!
//! @param pHandle             - Pointer to driver handle.
//! @param pui8RxBuffer        - Buffer to receive data.
//! @param ui32NumBytes        - Number of bytes to read.
//! @param bWaitForCompletion  - When true, block until transfer completes.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_read(void *pHandle,
                                                  uint8_t *pui8RxBuffer,
                                                  uint32_t ui32NumBytes,
                                                  bool bWaitForCompletion);

//*****************************************************************************
//
//! @brief Read the panel/device ID.
//!
//! Reads the display ID and stores it into the provided `pdata` pointer.
//!
//! @param pHandle - Pointer to driver handle.
//! @param pdata   - Pointer to a uint32_t where the ID will be stored.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_read_id(void *pHandle,
                                         uint32_t *pdata);

//*****************************************************************************
//
//! @brief Initialize the RM67162 MSPI display driver.
//!
//! Configures MSPI and prepares the display device for operation. Call before
//! using other RM67162 APIs.
//!
//! @param ui32Module       - MSPI module ID.
//! @param psMSPISettings   - Pointer to `am_devices_mspi_rm67162_config_t`.
//! @param ppHandle         - Pointer to location to receive device handle.
//! @param ppMspiHandle     - Pointer to location to receive MSPI handle.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm67162_init(uint32_t ui32Module,
                                             am_devices_mspi_rm67162_config_t *psMSPISettings,
                                             void **ppHandle,
                                             void **ppMspiHandle);

//*****************************************************************************
//
//! @brief De-initialize the RM67162 driver and release resources.
//!
//! @param pHandle - Pointer to device handle previously returned by init.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_term(void *pHandle);

//*****************************************************************************
//
//! @brief Send a generic command with optional data to the display.
//!
//! @param pHandle     - Pointer to driver handle.
//! @param ui32Instr   - Instruction/command opcode to send.
//! @param pData       - Pointer to data bytes to send after the command.
//! @param ui32NumBytes- Number of data bytes to send.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_command_write(void *pHandle,
                                               uint32_t ui32Instr,
                                               uint8_t *pData,
                                               uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Set the rectangle window for subsequent transfers.
//!
//! Defines the start/end rows and columns for block transfers to the panel.
//!
//! @param pHandle   - Pointer to driver handle.
//! @param startRow  - Starting row index (inclusive).
//! @param startCol  - Starting column index (inclusive).
//! @param endRow    - Ending row index (inclusive).
//! @param endCol    - Ending column index (inclusive).
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm67162_set_transfer_window(void *pHandle,
                                                            uint32_t startRow,
                                                            uint32_t startCol,
                                                            uint32_t endRow,
                                                            uint32_t endCol);
#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_RM67162_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

