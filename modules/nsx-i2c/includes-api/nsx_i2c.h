#ifndef NSX_I2C
    #define NSX_I2C

/**
 * @file nsx_i2c.h
 * @author Ambiq
 * @brief Generic I2C driver
 * @version 0.1
 * @date 2022-08-26
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *  \addtogroup nsx-i2c
 *  @{
 */

    // Ambiq/CMSIS headers include overloaded MVE intrinsics in C++ mode.
    // Keep them out of any caller-provided extern "C" block.
    #ifdef __cplusplus
extern "C++" {
    #endif
    #include "am_mcu_apollo.h"
    #include "am_util.h"
    #ifdef __cplusplus
}
    #endif

    #include "nsx_core.h"

    #ifdef __cplusplus
extern "C" {
    #endif

    #define NSX_I2C_V0_0_1                                                                          \
        { .major = 0, .minor = 0, .revision = 1 }
    #define NSX_I2C_V1_0_0                                                                          \
        { .major = 1, .minor = 0, .revision = 0 }

    #define NSX_I2C_OLDEST_SUPPORTED_VERSION NSX_I2C_V0_0_1
    #define NSX_I2C_CURRENT_VERSION NSX_I2C_V1_0_0
    #define NSX_I2C_API_ID 0xCA0004

extern const nsx_core_api_t nsx_i2c_V0_0_1;
extern const nsx_core_api_t nsx_i2c_V1_0_0;
extern const nsx_core_api_t nsx_i2c_oldest_supported_version;
extern const nsx_core_api_t nsx_i2c_current_version;

typedef enum { NSX_I2C_STATUS_SUCCESS = 0, NSX_I2C_STATUS_ERROR = 1 } nsx_i2c_status_e;

/**
 * @brief i2c configuration
 *
 */
typedef struct {
    const nsx_core_api_t *api; ///< API prefix
    int8_t iom;               ///< Ambiq IOM port

    // Internal state
    void *iomHandle;             // AmbiqSuite IOM handle
    am_hal_iom_config_t sIomCfg; //  AmbiqSuite IOM config
} nsx_i2c_config_t;

// I2C Transfer Flags
typedef enum { NSX_I2C_XFER_WR = 0, NSX_I2C_XFER_RD = (1u << 0) } nsx_i2c_xfer_flag_e;

// I2C Transfer Message
typedef struct {
    uint16_t addr;
    uint16_t flags;
    uint16_t len;
    uint8_t *buf;
} nsx_i2c_msg_t;

/**
 * @brief Initialize I2C on one of the IOM (IO managers)
 * @param cfg I2C configuration
 * @param speed I2C speed in Hz
 * @return uint32_t status
 */
uint32_t nsx_i2c_interface_init(nsx_i2c_config_t *cfg, uint32_t speed);

/**
 * @brief Perform low-level I2C read using IOM transfer
 * @param cfg I2C configuration
 * @param buf Buffer to store read bytes
 * @param size Number of bytes to read
 * @param addr I2C device address
 */
uint32_t nsx_i2c_read(nsx_i2c_config_t *cfg, const void *buf, uint32_t size, uint16_t addr);

/**
 * @brief Perform low-level I2C write using IOM transfer
 * @param cfg I2C configuration
 * @param buf Buffer of bytes to write
 * @param size Number of bytes to write
 * @param addr I2C device address
 */
uint32_t nsx_i2c_write(nsx_i2c_config_t *cfg, const void *buf, uint32_t size, uint16_t addr);

/**
 * @brief Legacy write then read, with STOP between the two operations.
 * @param cfg I2C configuration
 * @param writeBuf Write buffer
 * @param numWrite Number of bytes to write
 * @param readBuf Read buffer
 * @param numRead Number of bytes to read
 * @param addr I2C device address
 */
uint32_t nsx_i2c_write_read(
    nsx_i2c_config_t *cfg, uint16_t addr, const void *writeBuf, size_t numWrite, void *readBuf,
    size_t numRead);

/** Maximum prefix and receive lengths for the combined operation on this SoC. */
#define NSX_I2C_COMBINED_MAX_WRITE_SIZE AM_HAL_IOM_MAX_OFFSETSIZE
#define NSX_I2C_COMBINED_MAX_READ_SIZE AM_HAL_IOM_MAX_TXNSIZE_I2C

/**
 * @brief Write a short command, repeated START, then read in one IOM transaction.
 *
 * Wire sequence: START, addr+W, writeBuf[0..numWrite-1], repeated START,
 * addr+R, numRead bytes (last byte NACK), STOP. No STOP separates the phases.
 * Prefix bytes retain their supplied order. Longer commands are rejected, not
 * split: Apollo3/3p support 1..3 bytes, newer supported IOMs support 1..5.
 * Both lengths must be nonzero and within the limits above. addr is an unshifted
 * 7-bit address. readBuf must be uint32_t-aligned and writable for numRead bytes;
 * writeBuf may be byte-aligned. Buffers are borrowed only until return.
 *
 * Requires an initialized I2C cfg. The caller serializes the entire operation
 * against all users of this IOM, including queued/DMA users; this API does not
 * acquire an RTOS lock and is not ISR-safe. IOM timeout/error recovery is owned
 * by the blocking HAL; this function adds neither retries nor a software-held
 * continuation. A failed operation may consume device data and partially write
 * readBuf. Do not assume a failed command is safe to replay.
 *
 * Returns AM_HAL_STATUS_SUCCESS, HAL INVALID_HANDLE/INVALID_ARG for malformed
 * inputs, HAL OUT_OF_RANGE for unsupported lengths, HAL INVALID_OPERATION for
 * a non-I2C cfg, or the original blocking HAL failure code. Unlike legacy I2C
 * helpers, this function preserves native HAL diagnostics. Invalid requests
 * fail before any bus activity, even if HAL API validation is disabled.
 */
uint32_t nsx_i2c_write_read_combined(
    nsx_i2c_config_t *cfg, uint16_t addr, const void *writeBuf, size_t numWrite,
    void *readBuf, size_t numRead);

/**
 * @brief Perform sequence of low-level I2C transfers (similar to Linux)
 * @param cfg I2C configuration
 * @param msgs I2C messages to transfer
 * @param numMsgs Number of I2C messsages
 */
uint32_t nsx_i2c_transfer(nsx_i2c_config_t *cfg, nsx_i2c_msg_t *msgs, size_t numMsgs);

    #ifdef __cplusplus
}
    #endif

#endif // NS_IO_I2C
/** @}*/
