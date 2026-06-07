//*****************************************************************************
//
//! @file am_devices_sdio_rs9116.h
//!
//! @brief SDIO WiFi driver for the RS9116 module.
//!
//! @addtogroup devices_sdio_rs9116 RS9116 SDIO WiFi Driver
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
#ifndef AM_DEVICES_SDIO_RS9116_H
#define AM_DEVICES_SDIO_RS9116_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Global definitions
//
//*****************************************************************************
extern am_hal_card_host_t *pSdhcCardHost;
extern am_hal_card_t       RS9116SdioCard;
extern uint32_t            ui32SdioModule;
extern uint8_t             sdio_init_done;

//*****************************************************************************
//
//! Global type definitions.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS = 0,
    AM_DEVICES_SDIO_RS9116_STATUS_ERROR  = -1,
} am_devices_sdio_rs9116_status_t;

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Write multiple SDIO blocks to the RS9116 device.
//!
//! @param tx_data - Pointer to the data to transmit.
//! @param Addr - Start block address to write to.
//! @param no_of_blocks - Number of blocks to write.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int16_t rsi_sdio_write_multiple(uint8_t *tx_data, uint32_t Addr, uint32_t no_of_blocks);

//*****************************************************************************
//
//! @brief Read multiple SDIO blocks from the RS9116 device.
//!
//! @param read_buff - Buffer to receive read data.
//! @param Addr - Start block address to read from.
//! @param no_of_blocks - Number of blocks to read.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int8_t rsi_sdio_read_multiple(uint8_t *read_buff, uint32_t Addr, uint32_t no_of_blocks);

//*****************************************************************************
//
//! @brief Write a single byte to an SDIO register.
//!
//! @param Addr - Register address to write.
//! @param dBuf - Pointer to the byte value to write.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int8_t sdio_reg_writeb(uint32_t Addr, uint8_t *dBuf);

//*****************************************************************************
//
//! @brief Read a single byte from an SDIO register.
//!
//! @param Addr - Register address to read.
//! @param dBuf - Pointer to buffer to receive the byte.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int8_t sdio_reg_readb(uint32_t Addr, uint8_t *dBuf);

//*****************************************************************************
//
//! @brief Read bytes from the RS9116 via SDIO.
//!
//! @param addr - Byte address to read from.
//! @param len - Number of bytes to read.
//! @param dBuf - Destination buffer for read data.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int16_t rsi_sdio_readb(uint32_t addr, uint16_t len, uint8_t *dBuf);

//*****************************************************************************
//
//! @brief Write bytes to the RS9116 via SDIO.
//!
//! @param addr - Byte address to write to.
//! @param len - Number of bytes to write.
//! @param dBuf - Source buffer for data to write.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int16_t rsi_sdio_writeb(uint32_t addr, uint16_t len, uint8_t *dBuf);

//*****************************************************************************
//
//! @brief Initialize the SDIO interface for the RS9116 module.
//!
//! This sets up the host controller and configures the RS9116 card for SDIO
//! operation.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int32_t rsi_mcu_sdio_init(void);

//*****************************************************************************
//
//! @brief Apply SDIO TX/RX timing adjustments.
//!
//! @param ui8TxRxDelays - Two-byte array containing TX and RX delay values.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int32_t rsi_sdio_apply_timing(uint8_t ui8TxRxDelays[2]);

//*****************************************************************************
//
//! @brief Perform a timing calibration scan for SDIO communication.
//!
//! @param eIndex - Host instance index.
//! @param eUHSMode - UHS mode to test.
//! @param ui32Clock - Clock frequency to use for the test.
//! @param eBusWidth - Bus width to test.
//! @param ui8CalibBuf - Buffer to store calibration results.
//! @param ui32StartAddr - Start address for test transfers.
//! @param ui32BlockCnt - Number of blocks to test.
//! @param eIoVoltage - IO voltage for the host.
//! @param ui8TxRxDelays - Two-byte array containing TX/RX delay values to try.
//! @param pSdioCardReset - Callback used to reset the SDIO card during scan.
//!
//! @return 32-bit status: `AM_DEVICES_SDIO_RS9116_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_SDIO_RS9116_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern int32_t rsi_sdio_timing_scan(am_hal_host_inst_index_e eIndex,
                           am_hal_host_uhs_mode_e eUHSMode,
                           uint32_t ui32Clock,
                           am_hal_host_bus_width_e eBusWidth,
                           uint8_t *ui8CalibBuf,
                           uint32_t ui32StartAddr,
                           uint32_t ui32BlockCnt,
                           am_hal_host_bus_voltage_e eIoVoltage,
                           uint8_t ui8TxRxDelays[2],
                           am_hal_sdio_card_reset_func pSdioCardReset);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_SDIO_RS9116_H

//*****************************************************************************
// End Doxygen group.
//! @}
//
//*****************************************************************************

