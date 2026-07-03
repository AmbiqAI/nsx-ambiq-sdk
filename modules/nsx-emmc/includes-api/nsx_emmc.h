#ifndef NSX_EMMC_H
#define NSX_EMMC_H

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

#define NSX_EMMC_V0_0_1 { .major = 0, .minor = 0, .revision = 1 }
#define NSX_EMMC_OLDEST_SUPPORTED_VERSION NSX_EMMC_V0_0_1
#define NSX_EMMC_CURRENT_VERSION NSX_EMMC_V0_0_1
#define NSX_EMMC_API_ID 0xCA0014

extern const nsx_core_api_t nsx_emmc_V0_0_1;
extern const nsx_core_api_t nsx_emmc_oldest_supported_version;
extern const nsx_core_api_t nsx_emmc_current_version;

//
// nsx-emmc is a thin, stable wrapper over AmbiqSuite's SDIO/eMMC HAL
// (am_hal_card_*/am_hal_sdhc_*), which is fully precompiled into
// libam_hal.a -- unlike nsx-nvm/nsx-psram, no vendored or directly
// referenced AmbiqSuite device-driver source is required.
//
// IMPORTANT (found empirically -- see README.md): on real apollo510_evb /
// apollo510b_evb hardware, the eMMC's VCC/VCCQ power rails are NOT powered
// by default. No AmbiqSuite reference example in this SDK's provider calls
// am_bsp_emmc_power_on() before touching SDIO; without it, card detection
// and every subsequent SDIO command hangs indefinitely waiting for a
// command-complete interrupt that never arrives, with no error returned.
// nsx_emmc_init() calls it automatically so callers don't have to
// rediscover this.
//

/// SDIO bus width.
typedef enum {
    NSX_EMMC_BUS_WIDTH_1 = 1,
    NSX_EMMC_BUS_WIDTH_4 = 4,
    NSX_EMMC_BUS_WIDTH_8 = 8,
} nsx_emmc_bus_width_e;

/**
 * @brief eMMC configuration.
 *
 * Fill in the fields below and call nsx_emmc_init(). Fields marked "out"
 * are populated by nsx_emmc_init() on success.
 */
typedef struct {
    const nsx_core_api_t *api; //!< Must point to nsx_emmc_current_version.

    bool enable; //!< Set true to enable/init the device.

    // SDIO/host selection
    uint8_t sdio_instance;               //!< SDIO host instance (0 or 1).
    nsx_emmc_bus_width_e bus_width;       //!< Bus width to negotiate with the card.
    uint32_t clock_freq_hz;               //!< e.g. 48000000.

    // out: filled in by nsx_emmc_init() on success.
    uint32_t block_count;  //!< Total number of 512-byte blocks on the card.
} nsx_emmc_config_t;

/**
 * @brief Populate a config struct with documented defaults.
 *
 * Sets api, enable = true, sdio_instance = 0, bus_width =
 * NSX_EMMC_BUS_WIDTH_4, clock_freq_hz = 48000000 (matching AmbiqSuite's
 * emmc_raw_block_read_write reference example's default SDR mode).
 */
uint32_t nsx_emmc_default_config(nsx_emmc_config_t *cfg);

/**
 * @brief Power on the eMMC device, find the card, initialize it, and
 * negotiate the configured bus width/clock.
 *
 * @param cfg Config struct (must remain valid during operation).
 * @return NSX_STATUS_SUCCESS on success, nsx_core error codes on validation
 *         errors, or the underlying HAL status code
 *         (AM_HAL_STATUS_*) on HAL error.
 */
uint32_t nsx_emmc_init(nsx_emmc_config_t *cfg);

/**
 * @brief Synchronously write one or more 512-byte blocks to the card.
 *
 * @param start_block Starting block number (512 bytes/block).
 * @param num_blocks   Number of consecutive blocks to write.
 * @param buf          Source buffer in SRAM (num_blocks * 512 bytes).
 * @return HAL status code (AM_HAL_STATUS_SUCCESS on success).
 */
uint32_t nsx_emmc_block_write(uint32_t start_block, uint32_t num_blocks, const uint8_t *buf);

/**
 * @brief Synchronously read one or more 512-byte blocks from the card.
 *
 * @param start_block Starting block number (512 bytes/block).
 * @param num_blocks   Number of consecutive blocks to read.
 * @param buf          Destination buffer in SRAM (num_blocks * 512 bytes).
 * @return HAL status code (AM_HAL_STATUS_SUCCESS on success).
 */
uint32_t nsx_emmc_block_read(uint32_t start_block, uint32_t num_blocks, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* NSX_EMMC_H */
