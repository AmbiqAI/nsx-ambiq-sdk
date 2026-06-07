//*****************************************************************************
//
//! @file am_devices_em9305.h
//!
//! @brief Support functions for the EM Micro EM9305 BTLE radio
//!
//! @addtogroup devices_em9305 EM9305 BTLE Radio Device Driver
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
#ifndef AM_DEVICES_EM9305_H
#define AM_DEVICES_EM9305_H

#include <stdbool.h>
#include "am_bsp.h"

#if !defined(AM_BSP_EM9305_SPI_MODULE)
#define AM_BSP_EM9305_SPI_MODULE            6
#define AM_BSP_GPIO_EM9305_INT              117
#define AM_BSP_EM9305_RADIO_INT_CHNL        0

// #### INTERNAL BEGIN ####
#if defined APOLLO5_ATE
#define AM_BSP_GPIO_EM9305_CS               149
#define AM_BSP_GPIO_EM9305_EN               93
#define AM_BSP_EM9305_NO_SWAP               1

#define AM_BSP_EM9305_IOM                   AM_HAL_PWRCTRL_PERIPH_IOM6
#define AM_BSP_EM9305_RADIO_INT_ISR         am_gpio0_607f_isr
#define AM_BSP_EM9305_RADIO_INT_IRQ         GPIO0_607F_IRQn
#endif
// #### INTERNAL END ####
#endif  // !defined(AM_BSP_EM9305_SPI_MODULE)


#define SPI_MODULE  AM_BSP_EM9305_SPI_MODULE

#if !defined(AM_BSP_EM9305_RADIO_INT_CHNL) || !defined(AM_BSP_EM9305_RADIO_INT_IRQ) || !defined(AM_BSP_EM9305_RADIO_INT_ISR)
    #error "must define AM_BSP_EM9305_RADIO_INT_CHNL and AM_BSP_EM9305_RADIO_INT_IRQ in BSP"
#endif

#define GPIO_INT_CHANNEL    AM_BSP_EM9305_RADIO_INT_CHNL
#define GPIO_INT_IRQ        AM_BSP_EM9305_RADIO_INT_IRQ
#define GPIO_INT_ISR        AM_BSP_EM9305_RADIO_INT_ISR
#define GPIO_INT            AM_BSP_GPIO_EM9305_INT

#define AM_DEVICES_EM9305_MAX_DEVICE_NUM        1

// 255 data + 3 header
#define EM9305_MAX_RX_PACKET                    258

// EM9305 NVM INFO Page1 is 8KB, address range is 0x402000 ~ 0x403FFF
#define EM9305_NVM_INFO_PAGE1_START_ADDR        0x402000
// BLE firmware version number length
#define EM9305_FW_VER_NUM_LEN                   4
// Invalid version number, it's the default value from flash
#define EM9305_FW_VER_INVALID                   0xFFFFFFFF
// The data length to read from NVM INFO Page 1
#define EM9305_NVM_INFO_READ_LEN                248
// NVM Main area
#define EM9305_NVM_MAIN_AREA                    0
// NVM Information area
#define EM9305_NVM_INFO_AREA                    1
// NVM Info area page 1
#define EM9305_NVM_INFO_AREA_PAGE_1             1

#define UINT16_TO_BYTE0(n)        ((uint8_t) (n))
#define UINT16_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))
#define UINT32_TO_BYTE0(n)        ((uint8_t) (n))
#define UINT32_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))
#define UINT32_TO_BYTE2(n)        ((uint8_t) ((n) >> 16))
#define UINT32_TO_BYTE3(n)        ((uint8_t) ((n) >> 24))
#define UINT32_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8)), ((uint8_t)((n) >> 16)), ((uint8_t)((n) >> 24))

//*****************************************************************************
//
//! Option set trim value for high frequency crystal oscillator if the value
//! is different from recommended crystal.
//!  AM_DEVICES_EM9305_HF_CRYSTAL_CUSTOM_TRIMS
//!      0 = No trim and use EM's default value.
//!      1 = Set the obtained trim value for high frequency crystal oscillator.
//!  Default: 0x25
//
//*****************************************************************************
#define AM_DEVICES_EM9305_HF_CRYSTAL_CUSTOM_TRIMS      0
#if (AM_DEVICES_EM9305_HF_CRYSTAL_CUSTOM_TRIMS)
// The default high frequency crystal trim value from EM9305
#define EM9305_HF_CRYSTAL_TRIM_VALUE_DEFAULT        (0x25)
#endif

#define am_devices_em9305_buffer(A)                                                  \
    union                                                                     \
    {                                                                         \
        uint32_t words[(A + 3) >> 2];                                         \
        uint8_t bytes[A];                                                     \
    }
#define WHILE_TIMEOUT_MS(expr, limit, timeout)     \
        {                                          \
            uint32_t ui32Timeout = 0;              \
            while (expr)                           \
            {                                      \
                if (ui32Timeout == (limit * 1000)) \
                {                                  \
                    timeout = 1;                   \
                    break;                         \
                }                                  \
                am_util_delay_us(1);               \
                ui32Timeout++;                     \
            }                                      \
        }

#define am_devices_em9305_buffer(A)               \
    union                                         \
    {                                             \
        uint32_t words[(A + 3) >> 2];             \
        uint8_t bytes[A];                         \
    }

#define AM_DEVICES_EM9305_RAW                      0x0
#define AM_DEVICES_EM9305_CMD                      0x1

#define HCI_VSC_CMD_HEADER_LENGTH                      4
#define HCI_VSC_CMD_LENGTH(n)                          (HCI_VSC_CMD_HEADER_LENGTH + n)

#define HCI_VSC_CMD(CMD, ...)                          {AM_DEVICES_EM9305_CMD, UINT16_TO_BYTE0(CMD##_CMD_OPCODE), UINT16_TO_BYTE1(CMD##_CMD_OPCODE), CMD##_CMD_LENGTH, ##__VA_ARGS__}
#define HCI_RAW_CMD(OPCODE, LEN, ...)                  {AM_DEVICES_EM9305_CMD, UINT16_TO_BYTE0(OPCODE), UINT16_TO_BYTE1(OPCODE), LEN, ##__VA_ARGS__}

#define HCI_VSC_ENTER_CM_CMD_LENGTH                     0
#define HCI_VSC_CRC_CALCULATE_CMD_LENGTH                8
#define HCI_VSC_NVM_ERASE_PAGE_CMD_LENGTH               2
#define HCI_VSC_SET_TX_POWER_LEVEL_CMD_LENGTH           1
#define HCI_VSC_NVM_ERASE_NVM_MAIN_CMD_LENGTH           0
#define HCI_VSC_START_TRANSMIT_TEST_CMD_LENGTH          6
#define HCI_VSC_END_TRANSMIT_TEST_CMD_LENGTH            0
#define HCI_VSC_SET_SLEEP_OPTION_CMD_LENGTH             1
#define HCI_VSC_SET_DEV_PUB_ADDR_CMD_LENGTH             6
#define HCI_VSC_READ_AT_ADDRESS_CMD_LENGTH              5
#define HCI_VSC_WRITE_AT_ADDRESS_CMD_LENGTH             134
#define HCI_VSC_ENTER_SLEEP_CMD_LENGTH                  0
#define HCI_VSC_CALCULATE_TRIM_VALUE_CMD_LENGTH         1
#define HCI_VSC_READ_PRODUCT_INFO_CMD_LENGTH            0



//*****************************************************************************
//
//! @brief EM9305 vendor-specific HCI command opcodes.
//!
//! Used with am_devices_em9305_command_write() for NVM, trim, sleep, etc.
//
//*****************************************************************************
typedef enum
{
    HCI_VSC_CRC_CALCULATE_CMD_OPCODE                 = 0xFC4E,
    HCI_VSC_ENTER_CM_CMD_OPCODE                      = 0xFC4F,
    HCI_VSC_EXIT_CM_CMD_OPCODE                       = 0xFC50,
    HCI_VSC_READ_AT_ADDRESS_CMD_OPCODE               = 0xFD01,
    HCI_VSC_WRITE_AT_ADDRESS_CMD_OPCODE              = 0xFD03,
    HCI_VSC_NVM_ERASE_NVM_MAIN_CMD_OPCODE            = 0xFD06,
    HCI_VSC_NVM_ERASE_PAGE_CMD_OPCODE                = 0xFD07,

    //Vendor Specific Command read product information
    HCI_VSC_READ_PRODUCT_INFO_CMD_OPCODE             = 0xFC01,
    //Vendor Specific Command set device public address
    HCI_VSC_SET_DEV_PUB_ADDR_CMD_OPCODE              = 0xFC43,
    //Vendor Specific Command set sleep option
    HCI_VSC_SET_SLEEP_OPTION_CMD_OPCODE              = 0xFC49,
    //Vendor Specific Command start transmitter test
    HCI_VSC_START_TRANSMIT_TEST_CMD_OPCODE           = 0xFCC1,
    //Vendor Specific Command end transmitter test
    HCI_VSC_END_TRANSMIT_TEST_CMD_OPCODE             = 0xFCC2,
    //Vendor Specific Command set Tx power level
    HCI_VSC_SET_TX_POWER_LEVEL_CMD_OPCODE            = 0xFCC4,
    //! Ambiq Vendor Specific Command trigger Apollo510 and EM9305 to enter sleep
    HCI_VSC_ENTER_SLEEP_CMD_OPCODE                   = 0xFCF0,
    //! Ambiq Vendor Specific Command to obtain the high frequency crystal trim value
    HCI_VSC_CALCULATE_TRIM_VALUE_CMD_OPCODE          = 0xFCF1,
    // Set local supported feature
    HCI_VSC_SET_LOCAL_SUP_FEAT_CMD_OPCODE            = 0xFFF2,
}vsc_opcode;

//*****************************************************************************
//
//! @brief Memory access width for EM9305 register/memory reads.
//
//*****************************************************************************
typedef enum
{
    //! 8 bit access types
    RD_8_Bit                   = 8,
    //! 16 bit access types
    RD_16_Bit                  = 16,
    //! 32 bit access types
    RD_32_Bit                  = 32

}eMemAccess_type;


//*****************************************************************************
//
//! @brief EM9305 device structure.
//!
//! Holds the Module index, handle, and flags used by the driver for
//! selected interface transactions to the EM9305.
//
//*****************************************************************************
typedef struct
{
    // IOM Module
    uint32_t ui32Module;
    // BLE IOM handle
    void     *pIomHandle;
    // Mark if BLE IOM bus is occupied
    bool     bOccupied;
    // Mark if the BLE interface is busy
    bool     bBusy;
}
am_devices_em9305_t;

//*****************************************************************************
//
//! @brief EM9305 driver return/status.
//
//*****************************************************************************
typedef enum
{
    AM_DEVICES_EM9305_STATUS_SUCCESS = 0,
    AM_DEVICES_EM9305_STATUS_ERROR,
    AM_DEVICES_EM9305_STATUS_BUS_BUSY,
    AM_DEVICES_EM9305_TX_BUSY,
    AM_DEVICES_EM9305_RESET_SPI_RDY_HIGH,
    AM_DEVICES_EM9305_RESET_FAIL,
    AM_DEVICES_EM9305_NOT_READY,
    AM_DEVICES_EM9305_NO_DATA_TX,
    AM_DEVICES_EM9305_RX_FULL,
    AM_DEVICES_EM9305_DATA_LENGTH_ERROR,
    AM_DEVICES_EM9305_CMD_TRANSFER_ERROR,
    AM_DEVICES_EM9305_DATA_TRANSFER_ERROR,
    AM_DEVICES_EM9305_STATUS_INVALID_OPERATION,
    AM_DEVICES_EM9305_INSUFFICIENT_LEN ,
    AM_DEVICES_EM9305_CKECKSUM_ERROR ,
    AM_DEVICES_EM9305_SEND_CMD_FAIL ,
    AM_DEVICES_EM9305_INCORRECT_DATA_TYPE,
    AM_DEVICES_EM9305_GET_MEM_INFO_FAIL,
    AM_DEVICES_EM9305_ALREADY_PROGRAMMED,

} am_devices_em9305_status_t;

typedef struct
{
    uint32_t* pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
} am_devices_em9305_config_t;

typedef struct
{
    const uint8_t *data;
    uint32_t length;
    uint32_t address;
} ImageRecord ;

typedef struct
{
    uint8_t page;
    uint8_t area;
}NvmPage;

//*****************************************************************************
//
//! @brief Configure the board pins used by the EM9305 interface.
//!
//! Set up the GPIO pinmux and initial pin states used to communicate with
//! the EM9305 radio.
//
//*****************************************************************************
extern void am_devices_em9305_config_pins(void);

//*****************************************************************************
//
//! @brief Initialize EM9305 device and IOM interface.
//!
//! This function initializes the EM9305 driver, powers up the device as
//! necessary and returns device and IOM handles via the provided pointers.
//!
//! @param ui32Module   IOM module to use for the EM9305 interface.
//! @param pDevConfig   Pointer to the device configuration structure.
//! @param ppHandle     Out pointer that will receive the device handle.
//! @param ppIomHandle  Out pointer that will receive the IOM handle.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR` (no free device handle, invalid
//!         module/config arguments, or IOM init/config/enable failure).
//
//*****************************************************************************
extern uint32_t am_devices_em9305_init(uint32_t ui32Module,
                                       am_devices_em9305_config_t *pDevConfig,
                                       void **ppHandle,
                                       void **ppIomHandle);

//*****************************************************************************
//
//! @brief Terminate the EM9305 driver and release resources.
//!
//! Shut down driver state and free any allocated resources used by the
//! driver.
//!
//! @param pHandle  Device handle returned by `am_devices_em9305_init`.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR` (invalid IOM module in handle).
//
//*****************************************************************************
extern uint32_t am_devices_em9305_term(void* pHandle);

//*****************************************************************************
//
//! @brief Write a block of bytes to the EM9305.
//!
//! This function writes `ui32NumBytes` from `pui8Values` to the EM9305 over
//! the selected interface.
//!
//! @param pHandle      Device handle.
//! @param pui8Values   Pointer to data to write.
//! @param ui32NumBytes Number of bytes to write.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` or
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY` (from
//!         `am_devices_em9305_bus_enable()`).
//!         `AM_DEVICES_EM9305_RX_FULL` (no room in EM9305 TX FIFO).
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR` (IOM transfer failure).
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR` (`ui32NumBytes` exceeds
//!         EM9305 buffer size).
//
//*****************************************************************************
extern uint32_t am_devices_em9305_block_write(void *pHandle,
                                              uint8_t *pui8Values,
                                              uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Read a block of data from the EM9305.
//!
//! This function reads data from the EM9305 into `pui32Values`. The number
//! of bytes read is returned in `ui32NumBytes` on success.
//!
//! @param pHandle       Device handle.
//! @param pui32Values   Pointer to buffer to receive read data.
//! @param ui32NumBytes  Pointer to variable that will receive the number of
//!                      bytes read.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_TX_BUSY` (SPI transmit in progress).
//!         `AM_DEVICES_EM9305_NO_DATA_TX` (RDY low, no data to read).
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` or
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY` (from
//!         `am_devices_em9305_bus_enable()`).
//!         `AM_DEVICES_EM9305_NOT_READY` (device not ready).
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR` (packet too large for
//!         buffer).
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR` (IOM status transfer fail).
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR` (IOM RX transfer fail).
//
//*****************************************************************************
extern uint32_t am_devices_em9305_block_read(void *pHandle,
                                             uint32_t *pui32Values,
                                             uint32_t *ui32NumBytes);

//*****************************************************************************
//
//! @brief Enable EM9305 interrupt handling.
//!
//! Configure and enable the radio interrupt as required by the platform.
//
//*****************************************************************************
extern void am_devices_em9305_enable_interrupt(void);

//*****************************************************************************
//
//! @brief Disable EM9305 interrupt handling.
//!
//! Disable the radio interrupt to stop event notifications.
//
//*****************************************************************************
extern void am_devices_em9305_disable_interrupt(void);

//*****************************************************************************
//
//! @brief Send a raw command to the EM9305 and receive a response.
//!
//! Send a vendor HCI/command buffer to the EM9305 and optionally receive
//! response words into the provided response buffer.
//!
//! @param pHandle           Device handle.
//! @param pui32Cmd          Pointer to command words to send.
//! @param ui32Length        Length in words of the command.
//! @param pui32Response     Pointer to receive response words (may be NULL).
//! @param pui32BytesReceived Pointer to receive the number of bytes returned.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` (invalid command or
//!         response pointer arguments).
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR` (response wait timeout).
//!         Errors propagated from `am_devices_em9305_block_write()` /
//!         `am_devices_em9305_block_read()`, such as
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`,
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`,
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_NOT_READY`, `AM_DEVICES_EM9305_RX_FULL`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_command_write(void* pHandle,
                                                uint32_t* pui32Cmd,
                                                uint32_t ui32Length,
                                                uint32_t* pui32Response,
                                                uint32_t* pui32BytesReceived);

//*****************************************************************************
//
//! @brief Update the EM9305 firmware image.
//!
//! If `force_update` is true, the firmware image will be written regardless
//! of the current version; otherwise the update may be skipped if not needed.
//!
//! @param pHandle       Device handle.
//! @param force_update  Force update even if the firmware appears up-to-date.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR`.
//!         `AM_DEVICES_EM9305_SEND_CMD_FAIL`.
//!         `AM_DEVICES_EM9305_CKECKSUM_ERROR`.
//!         Errors propagated from `am_devices_em9305_command_write()` through
//!         update helpers, such as
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`,
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`,
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_NOT_READY`, `AM_DEVICES_EM9305_RX_FULL`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`,
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//!         `AM_HAL_STATUS_*` may also be propagated from CRC verification.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_update_image(void *pHandle, bool force_update);

//*****************************************************************************
//
//! @brief Reset the EM9305 radio.
//!
//! Perform a hardware or software reset of the EM9305 as appropriate.
//!
//! @param pHandle Device handle.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` if `pHandle` is NULL.
//!         `AM_DEVICES_EM9305_RESET_SPI_RDY_HIGH` if RDY stayed high for 30 s
//!         after reset.
//!         `AM_DEVICES_EM9305_RESET_FAIL`.
//!         Errors propagated from `am_devices_em9305_block_read()`, such as
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`,
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`, `AM_DEVICES_EM9305_NOT_READY`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`,
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_reset(void *pHandle);

//*****************************************************************************
//
//! @brief Retrieve firmware image records.
//!
//! Populate `pFwImage` with pointers to firmware image records for use in
//! firmware update operations.
//!
//! @param pFwImage       Array to be filled with pointers to image records.
//! @param record_size    Maximum number of records the array can hold.
//! @param total_image_len Total length of the image expected.
//!
//! @return true if the image records were found and populated, false
//!         otherwise.
//
//*****************************************************************************
extern bool am_devices_em9305_get_FwImage(ImageRecord *pFwImage[],
                                          uint8_t record_size,
                                          uint32_t total_image_len);

//*****************************************************************************
//
//! @brief Erase one or more NVM pages.
//!
//! Erase the specified NVM pages on the EM9305 non-volatile memory.
//!
//! @param pErasePage Pointer to an NVM page description to erase.
//! @param size       Number of pages described by `pErasePage`.
//!
//! @return true on success, false on failure.
//
//*****************************************************************************
extern bool am_devices_em9305_erase_page(NvmPage *pErasePage, uint8_t size);

//*****************************************************************************
//
//! @brief Get the firmware version from the device.
//!
//! Read and return the firmware version reported by the EM9305 device.
//!
//! @param pHandle    Device handle.
//! @param image_ver  Pointer to uint32_t to receive the firmware version.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR` (invalid pointer arguments in the
//!         internal read command path).
//!         `AM_DEVICES_EM9305_SEND_CMD_FAIL`.
//!         Errors propagated from `am_devices_em9305_command_write()`, such as
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`,
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`,
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_NOT_READY`, `AM_DEVICES_EM9305_RX_FULL`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`,
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_get_fw_verion(void *pHandle, uint32_t *image_ver);

//*****************************************************************************
//
//! @brief Update stored firmware version value.
//!
//! Update the driver's stored firmware version value for this device.
//!
//! @param pHandle   Device handle.
//! @param image_ver Firmware version number to store.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR` (internal read/write command path
//!         argument or command failure).
//!         `AM_DEVICES_EM9305_SEND_CMD_FAIL`.
//!         Errors propagated from `am_devices_em9305_command_write()`, such as
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`,
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`,
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_NOT_READY`, `AM_DEVICES_EM9305_RX_FULL`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`,
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_upate_fw_version(void *pHandle, uint32_t image_ver);

//*****************************************************************************
//
//! @brief Enter command mode.
//!
//! Place the device into the vendor-specific command mode for advanced ops.
//!
//! @param pHandle Device handle.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `am_devices_em9305_block_read()` errors may be observed internally,
//!         but this routine retries until configuration mode is entered.
//
//*****************************************************************************
extern uint32_t enter_cm_mode(void *pHandle);

//*****************************************************************************
//
//! @brief Request or release the high-speed clock.
//!
//! Request or release the EM9305 high-speed clock used for high-rate operations.
//!
//! @param enable true to request HSCLK, false to release it.
//
//*****************************************************************************
extern void am_devices_em9305_hsclk_req(bool enable);

//*****************************************************************************
//
//! @brief Disable the EM9305 interface.
//!
//! Power down or disable the EM9305 peripheral interface.
//
//*****************************************************************************
extern void am_devices_em9305_disable(void);

//*****************************************************************************
//
//! @brief Set the EM9305 pump voltage to 1.9V when required.
//!
//! Adjust the internal pump voltage to 1.9V when required for reliable operation.
//!
//! @param flag true to set pump to 1.9V, false to restore default.
//
//*****************************************************************************
extern void am_devices_em9305_set_pump_to_1p9v(bool flag);

//*****************************************************************************
//
//! @brief Retrieve firmware image for SPI swap operations.
//!
//! Populate the provided array with firmware image records used for SPI swap.
//!
//! @param pFwImage    Array to receive pointers to image records.
//! @param record_size Number of records the array can hold.
//! @param total_image_len Expected total image length.
//!
//! @return true if image records were found.
//
//*****************************************************************************
extern bool am_devices_em9305_get_swap_spi_FwImage(ImageRecord *pFwImage[],
                                                   uint8_t record_size,
                                                   uint32_t total_image_len);

//*****************************************************************************
//
//! @brief Erase SPI swap pages used during firmware swap operations.
//!
//! Erase specified SPI swap pages used during firmware swap operations.
//!
//! @param pErasePage Pointer to page descriptors.
//! @param size       Number of pages to erase.
//!
//! @return true on success.
//
//*****************************************************************************
extern bool am_devices_em9305_swap_spi_erase_page(NvmPage *pErasePage,
                                                  uint8_t size);

//*****************************************************************************
//
//! @brief Check whether SPI swap version is enabled.
//!
//! Check whether SPI swap versioning is enabled on the device.
//!
//! @return true if SPI swap versioning is enabled.
//
//*****************************************************************************
extern bool am_devices_em9305_spi_swap_version_enabled(void);

//*****************************************************************************
//
//! @brief Configure SPI bit-bang mode.
//!
//! When enabled, the driver will use software bit-banging for SPI instead of
//! the hardware IOM interface.
//!
//! @param bitbang_mode true to enable bit-bang mode, false to disable.
//
//*****************************************************************************
extern void am_devices_em9305_set_spi_bitbang_mode(bool bitbang_mode);

//*****************************************************************************
//
//! @brief Query whether SPI bit-bang mode is currently enabled.
//!
//! @return true if bit-bang SPI is enabled.
//
//*****************************************************************************
extern bool am_devices_em9305_spi_bitbang_enabled(void);

//*****************************************************************************
//
//! @brief Enable or disable device sleep mode.
//!
//! Enable or disable device sleep mode to control power state.
//!
//! @param pHandle Device handle.
//! @param enable  true to enable sleep, false to disable.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION`.
//!         `AM_DEVICES_EM9305_CMD_TRANSFER_ERROR`.
//!         Errors propagated from `am_devices_em9305_block_write()` /
//!         `am_devices_em9305_block_read()`, such as
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY`,
//!         `AM_DEVICES_EM9305_TX_BUSY`, `AM_DEVICES_EM9305_NO_DATA_TX`,
//!         `AM_DEVICES_EM9305_NOT_READY`, `AM_DEVICES_EM9305_RX_FULL`,
//!         `AM_DEVICES_EM9305_DATA_LENGTH_ERROR`, and
//!         `AM_DEVICES_EM9305_DATA_TRANSFER_ERROR`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_sleep_set(void* pHandle, bool enable);

//*****************************************************************************
//
//! @brief Perform device shutdown.
//!
//! Place the EM9305 into a shutdown state for lowest power.
//
//*****************************************************************************
extern void am_devices_em9305_shutdown(void);

//*****************************************************************************
//
//! @brief Increment or set the request counter used internally.
//!
//! @param bIncrement true to increment the counter, false to reset/clear.
//
//*****************************************************************************
extern void am_devices_em9305_request_counter_set(bool bIncrement);

//*****************************************************************************
//
//! @brief Retrieve the current request counter value.
//!
//! @return Current request counter (uint32_t).
//
//*****************************************************************************
extern uint32_t am_devices_em9305_request_counter_get(void);

// #### INTERNAL BEGIN ####
// FIXME: CC
//
//  IOM definitions for device drivers need to happen in the BSP. Change this
//      to am_devices_em9305_config_iom_default();
//  Also need to change implementation here to be more dependent on BSP. We
//      cannot just hardcode the IOM6 here.
//
// #### INTERNAL END ####

//*****************************************************************************
//
//! @brief Provide a default IOM6 configuration for the EM9305 driver.
//!
//! This helper configures a sensible default IOM6 configuration suitable for
//! the EM9305 peripheral when BSP-provided defaults are not available.
//
//*****************************************************************************
extern void am_devices_em9305_config_iom6_default(void);

//*****************************************************************************
//
//! @brief Enable the IOM bus for EM9305 operations.
//!
//! This function enables and configures the IOM module so that HCI/command
//! operations can be performed with the EM9305 radio.
//!
//! @param pHandle  Pointer to the BLE controller device handle.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` if handle is not
//!         occupied.
//!         `AM_DEVICES_EM9305_STATUS_BUS_BUSY` if bus is already busy.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_bus_enable(void* pHandle);

//*****************************************************************************
//
//! @brief Disable the IOM bus used by the EM9305.
//!
//! This function disables the IOM module used for communication with the
//! EM9305 to conserve power when HCI operations are complete.
//!
//! @param pHandle  Pointer to the BLE controller device handle.
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_INVALID_OPERATION` if handle is not
//!         occupied.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_bus_disable(void* pHandle);

//*****************************************************************************
//
//! @brief Read the EM9305 IRQ register value.
//!
//! This convenience function reads and returns the current EM9305 IRQ
//! register value for use by the caller; it does not return a driver
//! status code.
//!
//! @return 32-bit IRQ register value read from the EM9305.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_irq_read(void);


//*****************************************************************************
//
//! @brief Set the high frequency crystal trim value based on design tested value
//!
//! Set the HF crystal trim to a tested value to improve frequency accuracy.
//!
//! @param pHandle          - Pointer to device handle
//! @param trimValue        - The new tested trim value, it's 6 bits in the register
//                             default value is 0x25
//! @param force_update     - Indicate if force update trim value or not
//!
//! @return `AM_DEVICES_EM9305_STATUS_SUCCESS` on success.
//!         On failure:
//!         `AM_DEVICES_EM9305_STATUS_ERROR` (configure-mode or trim read/write
//!         update flow failed).
//!         `AM_DEVICES_EM9305_RESET_FAIL`.
//
//*****************************************************************************
extern uint32_t am_devices_em9305_crystal_trim_set(void *pHandle,
                                                   uint8_t trimValue,
                                                   bool force_update);

#endif // AM_DEVICES_EM9305_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
