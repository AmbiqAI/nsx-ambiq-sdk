//*****************************************************************************
//
//! @file am_devices_rm67162.c
//!
//! @brief Generic Raydium OLED display driver.
//!
//! @addtogroup rm67162 RM67162 Display Driver
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
//! @name Global definitions for the commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_RM67162_4SPI_INPUT_DATA_SEL           0xE4
#define AM_DEVICES_RM67162_MTP_LOAD_CTRL                 0xEB
#define AM_DEVICES_RM67162_BOOSTER_ENABLE                0xD1
#define AM_DEVICES_RM67162_GATE_VOL_CTRL                 0xC0
#define AM_DEVICES_RM67162_FRAME_RATE_CTRL               0xB2
#define AM_DEVICES_RM67162_UPDATE_PERIOD_GATE_EQ_CTRL    0xB4
#define AM_DEVICES_RM67162_DUTY_SETTING                  0xB0
#define AM_DEVICES_RM67162_SLEEP_OUT                     0x11
#define AM_DEVICES_RM67162_OSC_ENABLE                    0xC7
#define AM_DEVICES_RM67162_VCOMH_VOL_CTRL                0xCB
#define AM_DEVICES_RM67162_SCAN_MODE                     0x36
#define AM_DEVICES_RM67162_DATA_FORMAT_SEL               0x3A
#define AM_DEVICES_RM67162_DESTRESS_PERIOD_GATE_EQ_CTRL  0xB5
#define AM_DEVICES_RM67162_SOURCE_SETTING                0xB9
#define AM_DEVICES_RM67162_PANEL_SETTING                 0xB8
#define AM_DEVICES_RM67162_COLUMN_ADDR_SETTING           0x2A
#define AM_DEVICES_RM67162_ROW_ADDR_SETTING              0x2B
#define AM_DEVICES_RM67162_TEARING_EFFECT_LINE_ON        0x35
#define AM_DEVICES_RM67162_SET_TEAR_SCANLINE             0x44
#define AM_DEVICES_RM67162_HIGH_POWER_MODE_ON            0x38
#define AM_DEVICES_RM67162_DISPLAY_OFF                   0x28
#define AM_DEVICES_RM67162_DISPLAY_ON                    0x29
#define AM_DEVICES_RM67162_INVERSION_ON                  0x21
#define AM_DEVICES_RM67162_LOW_POWER_MODE_ON             0x39
#define AM_DEVICES_RM67162_MEMORY_WRITE                  0x2C
#define AM_DEVICES_RM67162_MEMORY_WRITE_CONTINUE         0x3C
#define AM_DEVICES_RM67162_MEMORY_READ                   0x2E
#define AM_DEVICES_RM67162_MEMORY_READ_CONTINUE          0x3E
#define AM_DEVICES_RM67162_READ_ID                       0x04
#define AM_DEVICES_RM67162_SET_DSPI_MODE                 0xC4
#define AM_DEVICES_RM67162_SET_WRITE_DISPLAY_CTRL        0x53

//! @}

//*****************************************************************************
//
// Global type definitions.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_RM67162_STATUS_SUCCESS,
    AM_DEVICES_RM67162_STATUS_ERROR
} am_devices_rm67162_status_t;

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

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Reset the RM67162 display device.
//!
//! Issues the hardware reset sequence to bring the panel into a known state.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_reset(void);

//*****************************************************************************
//
//! @brief Turn the display off (enter low-power/display-off state).
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_display_off(void);

//*****************************************************************************
//
//! @brief Turn the display on (enable display output / exit sleep).
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_display_on(void);

//*****************************************************************************
//
//! @brief Blocking write to the display memory.
//!
//! Copies `ui32NumBytes` from `pui8TxBuffer` to the display memory and blocks
//! until the transfer completes.
//!
//! @param pui8TxBuffer - Buffer containing data to write to the display.
//! @param ui32NumBytes - Number of bytes to write.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_blocking_write(uint8_t *pui8TxBuffer,
                                                  uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Non-blocking write to the display memory.
//!
//! Queues a write of `ui32NumBytes` from `pui8TxBuffer` to the display. If
//! `bContinue` is true the transfer will continue with subsequent queued ops.
//! Completion or errors will be reported via `pfnCallback` if provided.
//!
//! @param pui8TxBuffer   - Buffer containing data to write.
//! @param ui32NumBytes   - Number of bytes to write.
//! @param bContinue      - If true, keep CQ active for continued transfers.
//! @param pfnCallback    - Optional callback invoked on transfer completion.
//! @param pCallbackCtxt  - Context pointer passed to `pfnCallback`.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on successful
//!         queueing, `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_write(uint8_t *pui8TxBuffer,
                                                     uint32_t ui32NumBytes,
                                                     bool bContinue,
                                                     am_hal_iom_callback_t pfnCallback,
                                                     void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Advanced non-blocking write using the IOM command queue.
//!
//! Queues a write transaction with pre/post CQ conditions and an optional
//! completion callback.
//!
//! @param pui8TxBuffer       - Buffer containing data to write.
//! @param ui32NumBytes       - Number of bytes to write.
//! @param bContinue          - If true, keep CQ active for continued transfers.
//! @param ui32PauseCondition - Command-queue pause condition before execution.
//! @param ui32StatusSetClr   - Command-queue status set/clear condition after execution.
//! @param pfnCallback        - Optional callback invoked on transfer completion.
//! @param pCallbackCtxt      - Context pointer passed to `pfnCallback`.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on successful
//!         queueing, `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_write_adv(uint8_t *pui8TxBuffer, uint32_t ui32NumBytes, bool bContinue, uint32_t ui32PauseCondition, uint32_t ui32StatusSetClr, am_hal_iom_callback_t pfnCallback, void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Blocking read from the display memory.
//!
//! Reads `ui32NumBytes` of display data into `pui8RxBuffer` and blocks until
//! the transfer completes.
//!
//! @param pui8RxBuffer - Buffer to store the received data.
//! @param ui32NumBytes - Number of bytes to read.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_blocking_read(uint8_t *pui8RxBuffer, uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Non-blocking read from the display memory.
//!
//! Initiates a read of `ui32NumBytes` into `pui8RxBuffer`; completion is
//! reported via `pfnCallback` if provided.
//!
//! @param pui8RxBuffer   - Buffer to store the received data.
//! @param ui32NumBytes   - Number of bytes to read.
//! @param pfnCallback    - Optional callback invoked on transfer completion.
//! @param pCallbackCtxt  - Context pointer passed to `pfnCallback`.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on successful
//!         queueing, `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_nonblocking_read(uint8_t *pui8RxBuffer, uint32_t ui32NumBytes, am_hal_iom_callback_t pfnCallback, void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Read the display/device ID.
//!
//! Reads the display ID and stores it into the provided `pdata` pointer.
//!
//! @param pdata - Pointer to a uint32_t where the ID will be stored.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_read_id(uint32_t *pdata);

//*****************************************************************************
//
//! @brief Initialize the RM67162 display driver.
//!
//! Configures the IOM/MSPI interface and prepares the display device for
//! operation. Must be called before using other RM67162 APIs.
//!
//! @param ui32Module      - IOM/MSPI module ID.
//! @param psIOMSettings   - Pointer to an `am_hal_iom_config_t` describing
//!                         the IOM configuration.
//! @param psGraphic_conf  - Pointer to `am_devices_rm67162_graphic_conf_t`
//!                         describing panel geometry and modes.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_init(uint32_t ui32Module,
                                        am_hal_iom_config_t *psIOMSettings,
                                        am_devices_rm67162_graphic_conf_t *psGraphic_conf);

//*****************************************************************************
//
//! @brief De-initialize the RM67162 driver and release resources.
//!
//! @param ui32Module - IOM/MSPI module ID used during `init`.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_term(uint32_t ui32Module);

//*****************************************************************************
//
//! @brief Send a generic command to the display with optional data.
//!
//! @param bHiPrio   - When true, treat this command as high-priority.
//! @param ui32Instr - Instruction/command opcode to send.
//! @param bdata     - If true, send a data payload following the command.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_command_write(bool bHiPrio, uint32_t ui32Instr, bool bdata);

//*****************************************************************************
//
//! @brief Set the rectangle window for subsequent transfers.
//!
//! @param bHiPrio   - When true, use high-priority transfer path.
//! @param startRow  - Starting row index (inclusive).
//! @param startCol  - Starting column index (inclusive).
//! @param endRow    - Ending row index (inclusive).
//! @param endCol    - Ending column index (inclusive).
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_set_transfer_window(bool bHiPrio, uint32_t startRow, uint32_t startCol, uint32_t endRow, uint32_t endCol);

//*****************************************************************************
//
//! @brief Set the current write/read address to (x,y) on the panel.
//!
//! Configures the internal column/row address pointers before memory
//! read/write operations.
//!
//! @param x - Column index.
//! @param y - Row index.
//!
//! @return 32-bit status: `AM_DEVICES_RM67162_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_RM67162_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm67162_setting_address(uint32_t x, uint32_t y);

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

