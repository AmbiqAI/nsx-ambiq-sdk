//*****************************************************************************
//
//! @file am_devices_mspi_ds35x1ga.h
//!
//! @brief Multi-bit SPI NAND Flash driver for the DS35X1GA device.
//!
//! @addtogroup devices_mspi_ds35x1ga DS35X1GA MSPI NAND Driver
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

#ifndef AM_DEVICES_MSPI_DS35X1GA_H
#define AM_DEVICES_MSPI_DS35X1GA_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name  Global definitions for flash commands
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MSPI_DS35X1GA_PROGRAM_LOAD_X1 0x02
#define AM_DEVICES_MSPI_DS35X1GA_PROGRAM_LOAD_X4 0x32
#define AM_DEVICES_MSPI_DS35X1GA_PROGRAM_EXECUTE 0x10

#define AM_DEVICES_MSPI_DS35X1GA_READ_CELL_ARRAY 0x13
#define AM_DEVICES_MSPI_DS35X1GA_READ_BUFFER_X1  0x03
#define AM_DEVICES_MSPI_DS35X1GA_READ_BUFFER_X2  0x3B
#define AM_DEVICES_MSPI_DS35X1GA_READ_BUFFER_X4  0x6B

#define AM_DEVICES_MSPI_DS35X1GA_WRITE_DISABLE   0x04
#define AM_DEVICES_MSPI_DS35X1GA_WRITE_ENABLE    0x06

#define AM_DEVICES_MSPI_DS35X1GA_READ_ID         0x9F

#define AM_DEVICES_MSPI_DS35X1GA_BLOCK_ERASE     0xD8

#define AM_DEVICES_MSPI_DS35X1GA_SET_FEATURE     0x1F
#define AM_DEVICES_MSPI_DS35X1GA_GET_FEATURE     0x0F
#define AM_DEVICES_MSPI_DS35X1GA_FEATURE_STATUS  0xC0
#define AM_DEVICES_MSPI_DS35X1GA_FEATURE_B0      0xB0
#define AM_DEVICES_MSPI_DS35X1GA_FEATURE_A0      0xA0

#define AM_DEVICES_MSPI_DS35X1GA_RESET           0xFF
//! @}

//*****************************************************************************
//
//! @name  Device specific identification.
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MSPI_DS35X1GA_ID      0xE500
#define AM_DEVICES_MSPI_DS35X1GA_ID_MASK 0x00FF00
//! @}
//*****************************************************************************
//
//! @name  Device specific definitions for the flash size information
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MSPI_DS35X1GA_PAGE_DATA_SIZE 2048
#define AM_DEVICES_MSPI_DS35X1GA_PAGE_OOB_SIZE  64
#define AM_DEVICES_MSPI_DS35X1GA_PAGE_FULL_SIZE 2112 //Internal ECC is enabled, default; 64 bytes are used for redundancy or for other uses
#define AM_DEVICES_MSPI_DS35X1GA_BLOCK_SIZE     0x20000  //128K bytes
#define AM_DEVICES_MSPI_DS35X1GA_MAX_BLOCKS     1024
#define AM_DEVICES_MSPI_DS35X1GA_MAX_PAGES      65536
//! @}
//*****************************************************************************
//
//! @name Global definitions for the flash status register
//! @{
//
//*****************************************************************************
#define AM_DEVICES_DS35X1GA_ECCS  0x30  // ECC Status[1:0]
#define AM_DEVICES_DS35X1GA_PRG_F 0x08 // Program Fail
#define AM_DEVICES_DS35X1GA_ERS_F 0x04 // Erase Fail
#define AM_DEVICES_DS35X1GA_WEL   0x02   // Write enable latch
#define AM_DEVICES_DS35X1GA_OIP   0x01   // Operation in progress

#define AM_DEVICES_DS35X1GA_ECCS_NO_BIT_FLIPS            0x00
#define AM_DEVICES_DS35X1GA_ECCS_BIT_FLIPS_CORRECTED     0x10
#define AM_DEVICES_DS35X1GA_ECCS_BIT_FLIPS_NOT_CORRECTED 0x20
#define AM_DEVICES_DS35X1GA_ECCS_BIT_FLIPS_CORRECTED_THR 0x30 // more than the threshold bit
//! @}

//*****************************************************************************
//
//! @name Global definitions for the flash OTP register
//! @{
//
//*****************************************************************************
#define AM_DEVICES_DS35X1GA_OTP_PRT    0x80
#define AM_DEVICES_DS35X1GA_OTP_EN     0x40
#define AM_DEVICES_DS35X1GA_OTP_ECC_EN 0x10
#define AM_DEVICES_DS35X1GA_OTP_QC_EN  0x01
//! @}

//*****************************************************************************
//
//! @name Global definitions for the MSPI instance to use.
//! @{
//
//*****************************************************************************
#define AM_DEVICES_MSPI_DS35X1GA_MSPI_INSTANCE  0
#define AM_DEVICES_MSPI_DS35X1GA_MAX_DEVICE_NUM 1
//! @}
//*****************************************************************************
//
//! Global type definitions.
//
//*****************************************************************************
//
//!
//
typedef enum
{
    AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS,
    AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR
} am_devices_mspi_ds35x1ga_status_t;

//
//!
//
typedef struct
{
    am_hal_mspi_device_e eDeviceConfig;
    am_hal_mspi_clock_e eClockFreq;
    uint32_t *pNBTxnBuf;
    uint32_t ui32NBTxnBufLength;
    uint32_t ui32ScramblingStartAddr;
    uint32_t ui32ScramblingEndAddr;
} am_devices_mspi_ds35x1ga_config_t;

typedef struct
{
#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)
    uint32_t ui32Turnaround;
    uint32_t ui32Rxneg;
    uint32_t ui32Rxdqsdelay;
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO5_API)
    bool            bTxNeg;
    bool            bRxNeg;
    bool            bRxCap;
    uint8_t         ui8TxDQSDelay;
    uint8_t         ui8RxDQSDelay;
    uint8_t         ui8Turnaround;
#endif
} am_devices_mspi_ds35x1ga_sdr_timing_config_t;
typedef enum
{
    AM_DEVICES_MSPI_DS35X1GA_ECC_STATUS_NO_BIT_FLIPS = 0,
    AM_DEVICES_MSPI_DS35X1GA_ECC_STATUS_BIT_FLIPS_CORRECTED,
    AM_DEVICES_MSPI_DS35X1GA_ECC_STATUS_BIT_FLIPS_NOT_CORRECTED
} am_devices_mspi_ds35x1ga_ecc_status_t;
//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************
// ****************************************************************************
//
//! @brief Initialize the DS35X1GA MSPI NAND driver.
//!
//! This function initializes the driver and configures the MSPI interface
//! according to `psMSPISettings`. On success, it returns device and MSPI
//! handles via `ppHandle` and `ppMspiHandle`.
//!
//! @param ui32Module       MSPI module number to use for the device.
//! @param psMSPISettings   Pointer to the device configuration structure.
//! @param ppHandle         Pointer that will receive the device handle.
//! @param ppMspiHandle     Pointer that will receive the MSPI handle.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
// ****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_init(uint32_t ui32Module,
                                                const am_devices_mspi_ds35x1ga_config_t *psMSPISettings,
                                                void **ppHandle, void **ppMspiHandle);

// ****************************************************************************
//
//! @brief De-initialize the DS35X1GA MSPI NAND driver.
//!
//! This function releases resources associated with the supplied device
//! handle and returns the MSPI interface to an uninitialized state.
//!
//! @param pHandle  Device handle previously returned by init.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
// ****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_deinit(void *pHandle);

//*****************************************************************************
//
//! @brief Read the ID of the NAND flash.
//!
//! This function reads the device identification registers from the external
//! NAND and stores the result into `pui32DeviceID`.
//!
//! @param pHandle        DS35X1GA device handle.
//! @param pui32DeviceID  Pointer to a uint32_t to receive the device ID.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_id(void *pHandle, uint32_t *pui32DeviceID);

//*****************************************************************************
//
//! @brief Read a page from the NAND flash.
//!
//! This function reads the data region and OOB region of the specified page
//! into the provided buffers and returns an ECC result code via
//! `pui32EccResult`.
//!
//! @param pHandle         DS35X1GA device handle.
//! @param ui32PageNum     Page number to read (0..65535).
//! @param pui8DataBuffer  Buffer to receive the page data region.
//! @param ui32DataLen     Number of bytes to read from the data region.
//! @param pui8OobBuffer   Buffer to receive the OOB region data.
//! @param ui32OobLen      Number of bytes to read from the OOB region.
//! @param pui32EccResult  Pointer to receive ECC result (0=no flips,
//!                        1=corrected, 2=not corrected).
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_read(void *pHandle, uint32_t ui32PageNum,
                                                uint8_t *pui8DataBuffer,
                                                uint32_t ui32DataLen,
                                                uint8_t *pui8OobBuffer,
                                                uint32_t ui32OobLen,
                                                uint8_t *pui32EccResult);

//*****************************************************************************
//
//! @brief Write a page to the NAND flash.
//!
//! This function programs the data and OOB regions of the specified page
//! from the provided buffers.
//!
//! @param pHandle         DS35X1GA device handle.
//! @param ui32PageNum     Page number to program (0..65535).
//! @param pui8DataBuffer  Buffer containing data to write to the data region.
//! @param ui32DataLen     Number of bytes to write to the data region.
//! @param pui8OobBuffer   Buffer containing data to write to the OOB region.
//! @param ui32OobLen      Number of bytes to write to the OOB region.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_write(void *pHandle, uint32_t ui32PageNum,
                                                uint8_t *pui8DataBuffer,
                                                uint32_t ui32DataLen,
                                                uint8_t *pui8OobBuffer,
                                                uint32_t ui32OobLen);

//*****************************************************************************
//
//! @brief Erase a NAND flash block.
//!
//! This function erases the specified block in the external NAND device.
//!
//! @param pHandle       DS35X1GA device handle.
//! @param ui32BlockNum  Block number to erase (valid range: 0..1024).
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t am_devices_mspi_ds35x1ga_block_erase(void *pHandle, uint32_t ui32BlockNum);

//*****************************************************************************
//
//! @brief Apply SDR timing settings to the MSPI instance.
//!
//! This function programs the MSPI instance with the supplied SDR timing
//! configuration. It must be called after the MSPI instance has been
//! initialized and placed into a mode that allows fine delay programming.
//!
//! @param pHandle     Handle to the NAND flash device.
//! @param pDevSdrCfg  Pointer to the SDR timing configuration to apply.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success,
//!         `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t
am_devices_mspi_ds35x1ga_apply_sdr_timing(void *pHandle,
                                         am_devices_mspi_ds35x1ga_sdr_timing_config_t *pDevSdrCfg);

//*****************************************************************************
//
//! @brief Scan and determine MSPI SDR timing settings.
//!
//! This function scans through possible SDR delay/timing settings and
//! determines suitable TURNAROUND/RXNEG/RXDQSDELAY0 (or equivalent)
//! parameters for reliable operation. The resulting timing values are
//! returned in `pDevSdrCfg`.
//!
//! @param module     MSPI module number to test.
//! @param pDevCfg    Pointer to the device configuration used for testing.
//! @param pDevSdrCfg Pointer to the structure that will receive the selected
//!                   SDR timing configuration.
//!
//! @return `AM_DEVICES_MSPI_DS35X1GA_STATUS_SUCCESS` on success (results in
//!         `pDevSdrCfg`), `AM_DEVICES_MSPI_DS35X1GA_STATUS_ERROR` on failure.
//
//*****************************************************************************
extern uint32_t
am_devices_mspi_ds35x1ga_sdr_init_timing_check(uint32_t module,
                                              const am_devices_mspi_ds35x1ga_config_t *pDevCfg,
                                              am_devices_mspi_ds35x1ga_sdr_timing_config_t *pDevSdrCfg);

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_MSPI_DS35X1GA_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
