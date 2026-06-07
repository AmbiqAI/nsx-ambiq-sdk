//*****************************************************************************
//
//! @file am_devices_mspi_raydium.h
//!
//! @brief Multi-bit SPI Display driver for Raydium display panels.
//!
//! @addtogroup devices_mspi_raydium Raydium MSPI Display Driver
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

#ifndef AM_DEVICES_MSPI_RM69330_H
#define AM_DEVICES_MSPI_RM69330_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_mcu_apollo.h"
#include "am_util_stdio.h"
#include "am_util_delay.h"
//*****************************************************************************
//
//! @name Display Dimensions
//! @{
//
//*****************************************************************************
#define AM_DEVICES_RM69330_NUM_ROWS                         454
#define AM_DEVICES_RM69330_NUM_COLUMNS                      454
#define AM_DEVICES_RM69330_SCAN_MODE_ORDER_RGB              0x00
#define AM_DEVICES_RM69330_SCAN_MODE_ORDER_BGR              0x08

//! @}

//*****************************************************************************
//
//! @name Global definitions for DISPLAY commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MSPI_RM69330_CMD_WRITE                   0x02
#define AM_DEVICES_MSPI_RM69330_PIXEL_WRITE_ADDR1           0x32    // address on single line
#define AM_DEVICES_MSPI_RM69330_PIXEL_WRITE_ADDR4           0x12    // address on quad interface
#define AM_DEVICES_MSPI_RM69330_CMD_READ                    0x03
#define AM_DEVICES_MSPI_RM69330_READ_ID                     0x04

#define AM_DEVICES_MSPI_RM69330_SOFTWARE_RESET              0x01
#define AM_DEVICES_MSPI_RM69330_PAGE_PROGRAM                0x02
#define AM_DEVICES_MSPI_RM69330_READ                        0x03
#define AM_DEVICES_MSPI_RM69330_WRITE_DISABLE               0x04
#define AM_DEVICES_MSPI_RM69330_READ_STATUS                 0x05
#define AM_DEVICES_MSPI_RM69330_WRITE_ENABLE                0x06
#define AM_DEVICES_MSPI_RM69330_FAST_READ                   0x0B
#define AM_DEVICES_MSPI_RM69330_READ_PIXEL_FORMAT           0x0C
#define AM_DEVICES_MSPI_RM69330_SLEEP_IN                    0x10
#define AM_DEVICES_MSPI_RM69330_SLEEP_OUT                   0x11
#define AM_DEVICES_MSPI_RM69330_NORMAL_MODE_ON              0x13
#define AM_DEVICES_MSPI_RM69330_INVERSION_OFF               0x20
#define AM_DEVICES_MSPI_RM69330_DISPLAY_OFF                 0x28
#define AM_DEVICES_MSPI_RM69330_DISPLAY_ON                  0x29
#define AM_DEVICES_MSPI_RM69330_SET_COLUMN                  0x2A
#define AM_DEVICES_MSPI_RM69330_SET_ROW                     0x2B
#define AM_DEVICES_MSPI_RM69330_MEM_WRITE                   0x2C
#define AM_DEVICES_MSPI_RM69330_TE_LINE_OFF                 0x34
#define AM_DEVICES_MSPI_RM69330_TE_LINE_ON                  0x35
#define AM_DEVICES_MSPI_RM69330_SCAN_DIRECTION              0x36
#define AM_DEVICES_MSPI_RM69330_IDLE_MODE_OFF               0x38
#define AM_DEVICES_MSPI_RM69330_PIXEL_FORMAT                0x3A
#define AM_DEVICES_MSPI_RM69330_DUAL_READ                   0x3B
#define AM_DEVICES_MSPI_RM69330_MEM_WRITE_CONTINUE          0x3C
#define AM_DEVICES_MSPI_RM69330_SET_TEAR_SCANLINE           0x44
#define AM_DEVICES_MSPI_RM69330_WRITE_DISPLAY_BRIGHTNESS    0x51
#define AM_DEVICES_MSPI_RM69330_WRITE_ENHVOL_CFG            0x61
#define AM_DEVICES_MSPI_RM69330_RESET_ENABLE                0x66
#define AM_DEVICES_MSPI_RM69330_QUAD_READ                   0x6B
#define AM_DEVICES_MSPI_RM69330_WRITE_VOL_CFG               0x81
#define AM_DEVICES_MSPI_RM69330_RESET_MEMORY                0x99
#define AM_DEVICES_MSPI_RM69330_ENTER_4B                    0xB7
#define AM_DEVICES_MSPI_RM69330_SET_DSPI_MODE               0xC4
#define AM_DEVICES_MSPI_RM69330_BULK_ERASE                  0xC7
#define AM_DEVICES_MSPI_RM69330_SECTOR_ERASE                0xD8
#define AM_DEVICES_MSPI_RM69330_EXIT_4B                     0xE9
#define AM_DEVICES_MSPI_RM69330_QUAD_IO_READ                0xEB
#define AM_DEVICES_MSPI_RM69330_READ_QUAD_4B                0xEC
#define AM_DEVICES_MSPI_RM69330_CMD_MODE                    0xFE

#define AM_DEVICES_MSPI_RM69330_SPI_WRAM                    0x80
#define AM_DEVICES_MSPI_RM69330_DSPI_WRAM                   0x81

#define AM_DEVICES_MSPI_RM69330_COLOR_MODE_8BIT             0x72
#define AM_DEVICES_MSPI_RM69330_COLOR_MODE_3BIT             0x73
#define AM_DEVICES_MSPI_RM69330_COLOR_MODE_16BIT            0x75
#define AM_DEVICES_MSPI_RM69330_COLOR_MODE_18BIT            0x76
#define AM_DEVICES_MSPI_RM69330_COLOR_MODE_24BIT            0x77

#define AM_DEVICES_MSPI_RM69330_SCAN_MODE_0                 0x40
#define AM_DEVICES_MSPI_RM69330_SCAN_MODE_90                0x70
#define AM_DEVICES_MSPI_RM69330_SCAN_MODE_180               0x10
#define AM_DEVICES_MSPI_RM69330_SCAN_MODE_270               0x00

#define AM_DEVICES_MSPI_RM69330_MAX_DEVICE_NUM              1
//! @}

//*****************************************************************************
//
//! @name Global type definitions.
//! @{
//
//*****************************************************************************

typedef enum
{
    AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS,
    AM_DEVICES_MSPI_RM69330_STATUS_ERROR
} am_devices_mspi_rm69330_status_t;
typedef struct
{
    uint8_t ui8BusMode;
    uint8_t ui8ColorMode;
    uint8_t ui8ScanMode;

    uint16_t ui16Height;
    uint16_t ui16Width;
    uint16_t ui16RowOffset;
    uint16_t ui16ColumnOffset;
} am_devices_mspi_rm69330_graphic_conf_t;

typedef struct
{
    am_hal_mspi_device_e eDeviceConfig;
    am_hal_mspi_clock_e eClockFreq;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
} am_devices_mspi_rm69330_config_t;
//! @}

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Set the flip along with the x/y-axis or both the x and y-axis.
//!
//! This function configures the display's orientation and color order
//! according to the provided bitfield.
//!
//! @param ui8FlipXY            - how to flip the display
//!
//! @note Register bits function for Driver IC RM69330 or CO5300.
//!       Bitfield 0 - Reserved                 (please don't set this bit)
//!       Bitfield 1 - Flip along with y-axis   (reserved for CO5300)
//!       Bitfield 2 - Reserved
//!       Bitfield 3 - RGB or BGR order
//!       Bitfield 4 - Flip along with x-axis   (reserved for CO5300)
//!       Bitfield 5 - Flip along y = x         (reserved for CO5300)
//!       Bitfield 6 - Flip along with y-axis
//!       Bitfield 7 - Flip along with x-axis
//! The x-axis will keep no flip when setting bitfields 4 and 7 same time. In the
//! same way, the same is true for the y-axis. Please be careful some of the flips
//! could cause the tear effect.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_flip(void *pHandle, uint8_t ui8FlipXY);

//*****************************************************************************
//
//! @brief Initialize the MSPI_RM69330 driver.
//!
//! @param ui32Module       - MSPI module.
//! @param pDevCfg          - device configuration.
//! @param ppHandle         - MSPI module handle
//! @param ppMspiHandle     - MSPI handle's handle.
//!
//! This function should be called before any other am_devices_MSPI_RM69330
//! functions. It is used to set tell the other functions how to communicate
//! with the external screen hardware.
//!
//! The \e pfnWriteFunc and \e pfnReadFunc variables may be used to provide
//! alternate implementations of SPI write and read functions respectively. If
//! they are left set to 0, the default functions am_hal_iom_spi_write() and
//! am_hal_iom_spi_read() will be used.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_init(uint32_t ui32Module,
                                             am_devices_mspi_rm69330_config_t *pDevCfg,
                                             void **ppHandle,
                                             void **ppMspiHandle);

//*****************************************************************************
//
//! @brief De-Initialize the mspi_rm69330 driver.
//!
//! @param pHandle              - mspi handle
//!
//! This function reverses the initialization
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_term(void *pHandle);

//*****************************************************************************
//
//! @brief Turn the display off.
//!
//! This function issues the necessary commands to place the panel into a
//! low-power/off state.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_display_off(void *pHandle);

//*****************************************************************************
//
//! @brief Turn the display on.
//!
//! This function issues the necessary commands to bring the panel out of
//! low-power mode and enable normal display operation.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_display_on(void *pHandle);

//*****************************************************************************
//
//! @brief set recommended scanline.
//!
//! @param pHandle                       - mspi handle
//! @param TETimesPerFrame            - how many TE intervals transfer one frame
//!
//! This function used to set recommended scanline,it's valid when TETimesPerFrame
//! equal to 1 or 2.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_set_scanline_recommended_parameter(void *pHandle,
                                                                           uint8_t TETimesPerFrame);

//*****************************************************************************
//
//! @brief set refresh scanline.
//!
//! @param pHandle                       - mspi handle
//! @param ui16ScanLine                  - scanline index
//!
//! This function used to set scanline. It will return an error when the
//! specified scanline is outside the display's valid range.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.

//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_set_scanline(void *pHandle,
                                                     uint16_t ui16ScanLine);

//*****************************************************************************
//
//! @brief Programs the given range of display addresses.
//!
//! This function copies data from `pui8TxBuffer` into the display memory
//! via MSPI. It will transfer exactly `ui32NumBytes` bytes and may use CQ/DMA
//! depending on configuration.
//!
//! @param pHandle              - MSPI handle
//! @param pui8TxBuffer         - Buffer to write the data from
//! @param ui32NumBytes         - Number of bytes to write to the display memory
//! @param bWaitForCompletion   - Waits for CQ/DMA to complete before return.
//! @param bContinue            - memory write or write-continue flag.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_nonblocking_write(void *pHandle,
                                                          const uint8_t *pui8TxBuffer,
                                                          uint32_t ui32NumBytes,
                                                          bool bWaitForCompletion,
                                                          bool bContinue);

//! @brief Programs the given range of display addresses with advanced options.
//!
//! Similar to `am_devices_mspi_rm69330_nonblocking_write` but provides CQ
//! pause/set-clear conditions and a post-transaction callback.
//!
//! @param pHandle              - MSPI handle
//! @param pui8TxBuffer         - Buffer to write the data from
//! @param ui32NumBytes         - Number of bytes to write to the display memory
//! @param ui32PauseCondition   - CQ Pause condition before execution.
//! @param ui32StatusSetClr     - CQ Set/Clear condition after execution.
//! @param pfnCallback          - Callback function after execution.
//! @param pCallbackCtxt        - Callback context after execution.
//! @param bContinue            - memory write or write-continue flag.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_nonblocking_write_adv(void *pHandle,
                                                             uint8_t *pui8TxBuffer,
                                                             uint32_t ui32NumBytes,
                                                             uint32_t ui32PauseCondition,
                                                             uint32_t ui32StatusSetClr,
                                                             am_hal_mspi_callback_t pfnCallback,
                                                             void *pCallbackCtxt,
                                                             bool bContinue);

//*****************************************************************************
//
//! @brief Programs the given range of display addresses with optional endian swap.
//!
//! Similar to `am_devices_mspi_rm69330_nonblocking_write_adv` but optionally
//! swaps high/low bytes in half-word format during the transfer.
//!
//! @param pHandle              - MSPI handle
//! @param pui8TxBuffer         - Buffer to write the data from
//! @param ui32NumBytes         - Number of bytes to write to the display memory
//! @param ui32PauseCondition   - CQ Pause condition before execution.
//! @param ui32StatusSetClr     - CQ Set/Clear condition after execution.
//! @param pfnCallback          - Callback function after execution.
//! @param pCallbackCtxt        - Callback context after execution.
//! @param bContinue            - memory write or write-continue flag.
//! @param bReverseBytes        - reverse high-byte with low-byte in half-word format
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_nonblocking_write_endian(void *pHandle,
                                                                uint8_t *pui8TxBuffer,
                                                                uint32_t ui32NumBytes,
                                                                uint32_t ui32PauseCondition,
                                                                uint32_t ui32StatusSetClr,
                                                                am_hal_mspi_callback_t pfnCallback,
                                                                void *pCallbackCtxt,
                                                                bool bContinue,
                                                                bool bReverseBytes);

//*****************************************************************************
//
//! @brief Reset the row/column addressing state on the display controller.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_row_col_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Read the display controller/device ID.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//! @param pdata - Pointer to a uint32_t to receive the ID value.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_read_id(void *pHandle,
                                                uint32_t *pdata);

//*****************************************************************************
//
//! @brief Set the active transfer window (column/row range) for subsequent writes.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//! @param ui16ColumnStart - Start column index.
//! @param ui16ColumnSize - Number of columns in window.
//! @param ui16RowStart - Start row index.
//! @param ui16RowSize - Number of rows in window.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_set_transfer_window(void *pHandle,
                                                            uint16_t ui16ColumnStart,
                                                            uint16_t ui16ColumnSize,
                                                            uint16_t ui16RowStart,
                                                            uint16_t ui16RowSize);

//*****************************************************************************
//
//! @brief Configure driver parameters for subsequent transfers.
//!
//! Sets the transfer window and pixel format used by the driver for
//! subsequent write operations. This function configures only driver-side
//! parameters and does not issue MSPI transactions itself.
//!
//! @param ui16ColumnStart - Start column index.
//! @param ui16ColumnSize - Number of columns in window.
//! @param ui16RowStart - Start row index.
//! @param ui16RowSize - Number of rows in window.
//! @param ui8Format - Pixel format code (device-specific).
//
//*****************************************************************************
extern void am_devices_rm69330_set_parameters(uint16_t ui16ColumnStart,
                                              uint16_t ui16ColumnSize,
                                              uint16_t ui16RowStart,
                                              uint16_t ui16RowSize,
                                              uint8_t ui8Format);

//*****************************************************************************
//
//! @brief Read the current display pixel format or related format register.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//! @param pdata - Pointer to a uint32_t to receive the format value.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_rm69330_read_format(void *pHandle,
                                                    uint32_t *pdata);

//*****************************************************************************
//
//! @brief Retrieve current graphic configuration parameters from the driver.
//!
//! @param pHandle - Device handle returned from `am_devices_mspi_rm69330_init`.
//!
//! @return 32-bit status: `AM_DEVICES_MSPI_RM69330_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_RM69330_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_rm69330_get_parameters(void *pHandle);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_MSPI_RM69330_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

