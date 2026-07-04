#ifndef NSX_NVM_H
#define NSX_NVM_H

#ifdef __cplusplus
extern "C++" {
#endif
#include "am_mcu_apollo.h"
#ifdef __cplusplus
}
#endif

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_NVM_V0_0_1 { .major = 0, .minor = 0, .revision = 1 }
#define NSX_NVM_OLDEST_SUPPORTED_VERSION NSX_NVM_V0_0_1
#define NSX_NVM_CURRENT_VERSION NSX_NVM_V0_0_1
#define NSX_NVM_API_ID 0xCA0013

extern const nsx_core_api_t nsx_nvm_V0_0_1;
extern const nsx_core_api_t nsx_nvm_oldest_supported_version;
extern const nsx_core_api_t nsx_nvm_current_version;

//
// nsx-nvm is a thin, stable wrapper over the AmbiqSuite IS25WX064 MSPI NOR
// flash HAL to support erase/read/write and XIP enable/disable. It mirrors
// nsx-psram's lifecycle and IRQ-ownership shape: the MSPI IRQ line is
// registered with nsx-interrupt (see nsx_irq_register()) rather than the
// module reaching into NVIC directly, so app/board code retains control over
// interrupt priority policy and vector ownership.
//

//! Octal interface mode for the MSPI flash device.
typedef enum {
    NSX_NVM_IF_OCTAL = 0,       //!< AM_HAL_MSPI_FLASH_OCTAL_CE{0,1} (SDR)
    NSX_NVM_IF_OCTAL_DDR = 1,   //!< AM_HAL_MSPI_FLASH_OCTAL_DDR_CE{0,1}
    NSX_NVM_IF_OCTAL_1_8_8 = 2, //!< AM_HAL_MSPI_FLASH_OCTAL_CE{0,1}_1_8_8
} nsx_nvm_interface_e;

/**
 * @brief NVM configuration.
 *
 * Fill in the fields below and call nsx_nvm_init(). Fields marked "out"
 * are populated by nsx_nvm_init() on success.
 */
typedef struct {
    const nsx_core_api_t *api; //!< Must point to nsx_nvm_current_version.

    // Features
    bool enable;     //!< Set true to enable/init the device.
    bool enable_xip; //!< Enable XIP at the end of init (optional).

    // MSPI/device selection
    uint8_t chip_select;       //!< 0 -> CE0, 1 -> CE1.
    nsx_nvm_interface_e iface; //!< Plain octal SDR, octal DDR, or octal 1-8-8.
    am_hal_mspi_clock_e clock_freq; //!< e.g. AM_HAL_MSPI_CLK_48MHZ.

    // Optional non-blocking TCB buffer (if NULL, an internal one is used).
    uint32_t *nbtxn_buf;
    uint32_t nbtxn_buf_len;

    // Optional MSPI scrambling window (0 disables).
    uint32_t scrambling_start_addr;
    uint32_t scrambling_end_addr;

    // Whether nsx-nvm should configure the MPU region for its internal
    // DMA/TCB buffer. Set false if the app/board already owns MPU regions.
    bool configure_mpu;

    // out: filled in by nsx_nvm_init() on success.
    uint32_t xip_base_address; //!< XIP aperture base for the selected MSPI instance.
    uint32_t size_bytes;       //!< Device capacity in bytes.
} nsx_nvm_config_t;

/**
 * @brief Populate a config struct with documented defaults.
 *
 * Sets api, enable = true, enable_xip = false, configure_mpu = true,
 * chip_select = 0, iface = NSX_NVM_IF_OCTAL, and clock_freq =
 * AM_HAL_MSPI_CLK_48MHZ. These defaults match Ambiq's Apollo5 IS25WX064
 * validation path: plain octal SDR at 48 MHz. The Apollo510B BSP's MSPI reset
 * table provides valid GPIO mappings for both AM_HAL_MSPI_FLASH_OCTAL_CE0 and
 * AM_HAL_MSPI_FLASH_OCTAL_DDR_CE0 on module 1. Callers should still set fields
 * that depend on their use case (e.g. enable_xip) before calling
 * nsx_nvm_init().
 */
uint32_t nsx_nvm_default_config(nsx_nvm_config_t *cfg);

/**
 * @brief Initialize NVM per the configuration struct (timing scan + init).
 *
 * @param cfg Config struct (must remain valid during operation).
 * @return NSX_STATUS_SUCCESS on success, nsx_core error codes on validation
 *         errors, or the underlying HAL device status
 *         (AM_DEVICES_MSPI_IS25WX064_STATUS_*) on HAL error.
 */
uint32_t nsx_nvm_init(nsx_nvm_config_t *cfg);

/**
 * @brief Read from NVM into a buffer.
 *
 * @param addr Byte address in NVM.
 * @param buf  Destination buffer in SRAM.
 * @param len  Number of bytes to read.
 * @param wait If true, block until completion.
 * @return HAL device status code.
 */
uint32_t nsx_nvm_read(uint32_t addr, uint8_t *buf, uint32_t len, bool wait);

/**
 * @brief Write to NVM from a buffer. Caller must ensure erase has been done.
 *
 * @param addr Byte address in NVM (page boundaries handled by HAL).
 * @param buf  Source buffer in SRAM.
 * @param len  Number of bytes to write.
 * @param wait If true, block until completion.
 * @return HAL device status code.
 */
uint32_t nsx_nvm_write(uint32_t addr, const uint8_t *buf, uint32_t len, bool wait);

/**
 * @brief Erase a single sector containing the given address.
 *
 * @param sector_addr Sector-aligned (or any address within the sector).
 * @return HAL device status code.
 */
uint32_t nsx_nvm_sector_erase(uint32_t sector_addr);

/**
 * @brief Full chip erase. Use with care.
 * @return HAL device status code.
 */
uint32_t nsx_nvm_mass_erase(void);

/**
 * @brief Enable XIP on the MSPI instance configured during init.
 * @return HAL device status code.
 */
uint32_t nsx_nvm_enable_xip(void);

/**
 * @brief Disable XIP on the MSPI instance configured during init.
 * @return HAL device status code.
 */
uint32_t nsx_nvm_disable_xip(void);

#ifdef __cplusplus
}
#endif

#endif /* NSX_NVM_H */
