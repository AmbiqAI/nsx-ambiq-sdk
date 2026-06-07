#ifndef NSX_SPI
#define NSX_SPI
/**
 * @file nsx_spi.h
 * @author Ambiq
 * @brief Generic SPI driver
 * @version 0.1
 * @date 2022-08-26
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *  \addtogroup nsx-spi
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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { NSX_SPI_STATUS_SUCCESS = 0, NSX_SPI_STATUS_ERROR = 1 } nsx_spi_status_e;
struct nsx_spi_cfg;
typedef void (*nsx_spi_cb)(struct nsx_spi_cfg *);


// SPI Driver Configuration
typedef struct nsx_spi_cfg {
    int8_t iom; // Ambiq IOM port
    // Internal state
    void *iomHandle;             // AmbiqSuite IOM handle
    am_hal_iom_config_t sIomCfg; //  AmbiqSuite IOM config
    nsx_spi_cb cb;                // Callback
} nsx_spi_config_t;

// ISR Handler
void nsx_spi_handle_iom_isr (void);

/**
 * @brief Initialize the SPI interface
 *
 * @param cfg SPI Configuration
 * @param speed Bus speed in Hz
 * @param mode SPI mode (CPOL, CPHA)
 * @return uint32_t
 */
uint32_t nsx_spi_interface_init(nsx_spi_config_t *cfg, uint32_t speed, am_hal_iom_spi_mode_e mode);

/**
 * @brief Read from a SPI device. The SPI device is selected by the csPin parameter.
 *
 * @param cfg SPI Configuration
 * @param buf Buffer to read into
 * @param bufLen Buffer length
 * @param reg Register to read from
 * @param regLen Register length
 * @param csPin CS pin number
 * @return uint32_t
 */
uint32_t nsx_spi_read(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin);

/**
 * @brief Write to a SPI device. The SPI device is selected by the csPin parameter.
 *
 * @param cfg SPI Configuration
 * @param buf Buffer to write from
 * @param bufLen Buffer length
 * @param reg Register to write to
 * @param regLen Register length
 * @param csPin CS pin number
 * @return uint32_t
 */
uint32_t nsx_spi_write(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin);

/**
 * @brief Transfer data to/from a SPI device. The SPI device is selected by the csPin parameter.
 *
 * @param cfg SPI Configuration
 * @param txBuf Transmit buffer
 * @param rxBuf Receive buffer
 * @param size Size of the transfer
 * @param csPin CS pin number
 * @return uint32_t
 */
uint32_t nsx_spi_transfer(
    nsx_spi_config_t *cfg, const void *txBuf, const void *rxBuf, uint32_t size, uint32_t csPin);

/**
 * @brief Issure DMA read, the cfg->callback will be called when the transfer is complete
 *
 * @param cfg
 * @param buf
 * @param bufLen
 * @param reg
 * @param regLen
 * @param csPin
 * @return uint32_t
 */
uint32_t nsx_spi_read_dma(
    nsx_spi_config_t *cfg, const void *buf, uint32_t bufLen, uint64_t reg, uint32_t regLen,
    uint32_t csPin);

#ifdef __cplusplus
}
#endif

#endif // NSX_SPI
