#ifndef NSX_PSRAM_H
#define NSX_PSRAM_H

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_PSRAM_V1_0_0 { .major = 1, .minor = 0, .revision = 0 }
#define NSX_PSRAM_OLDEST_SUPPORTED_VERSION NSX_PSRAM_V1_0_0
#define NSX_PSRAM_CURRENT_VERSION NSX_PSRAM_V1_0_0
#define NSX_PSRAM_API_ID 0xCA0012

typedef enum {
    NSX_PSRAM_STATUS_NOT_INITIALIZED = 0x1200u,
    NSX_PSRAM_STATUS_OUT_OF_RANGE,
    NSX_PSRAM_STATUS_UNSUPPORTED,
    NSX_PSRAM_STATUS_ALREADY_INITIALIZED,
    NSX_PSRAM_STATUS_FAILED_STATE,
} nsx_psram_status_t;

#define NSX_PSRAM_CAP_SYNC_TRANSFER (1u << 0)
#define NSX_PSRAM_CAP_XIP           (1u << 1)
#define NSX_PSRAM_CAP_TIMING_SCAN   (1u << 2)

extern const nsx_core_api_t nsx_psram_V1_0_0;
extern const nsx_core_api_t nsx_psram_oldest_supported_version;
extern const nsx_core_api_t nsx_psram_current_version;

typedef enum {
    NSX_PSRAM_TIMING_UNAVAILABLE = 0,
    NSX_PSRAM_TIMING_NOT_RUN,
    NSX_PSRAM_TIMING_VALID,
    NSX_PSRAM_TIMING_FAILED,
} nsx_psram_timing_status_t;

typedef enum {
    NSX_PSRAM_STATE_UNINITIALIZED = 0,
    NSX_PSRAM_STATE_READY,
    NSX_PSRAM_STATE_FAILED,
} nsx_psram_state_t;

typedef struct {
    uint32_t base_address;
    uint32_t size_bytes;
    uint32_t configured_clock_hz;
    /**
     * Static capabilities supported by this platform build. These bits do not
     * describe the current initialization state, whether XIP was enabled for
     * this instance, or whether a timing scan ran or succeeded.
     */
    uint32_t capabilities;
    uint32_t last_init_status;
    nsx_psram_state_t state;
    bool xip_enabled;
    nsx_psram_timing_status_t timing_status;
    uint8_t rxdqs_delay;
} nsx_psram_info_t;

typedef struct {
    const nsx_core_api_t *api;
    uint32_t clock_hz;
    bool enable_xip;
    bool configure_mpu;
} nsx_psram_config_t;

uint32_t nsx_psram_default_config(nsx_psram_config_t *cfg);
uint32_t nsx_psram_init(const nsx_psram_config_t *cfg);
uint32_t nsx_psram_get_info(nsx_psram_info_t *info);

/**
 * Blocking PSRAM-to-SRAM transfer. Caller buffers may be reused after return.
 *
 * Apollo5 requires word-aligned PSRAM offsets; caller buffers and final lengths
 * need not be word-aligned. Transfers use a private 4 KiB DTCM buffer, with CPU
 * copies to the caller only after successful DMA completion. A failure may leave
 * an already copied prefix; the remainder is not valid transferred data.
 * Other families retain their device-driver alignment and cache requirements.
 *
 * Apollo5 transfer errors enter FAILED until reset. Retained DMA storage must
 * remain powered: do not enter deep sleep or power down DTCM after FAILED.
 * Concurrent Apollo5 blocking calls return the HAL IN_USE status.
 */
uint32_t nsx_psram_read(uint32_t offset, void *buffer, uint32_t length);

/**
 * Blocking SRAM-to-PSRAM transfer. Source buffers may be reused after return.
 *
 * Apollo5 copies through the same private DTCM storage and has the read API's
 * alignment, concurrency and FAILED-state restrictions. Errors may leave a
 * partially written PSRAM range. Neither operation cancels outstanding DMA on
 * error; its retained storage is quarantined until reset.
 *
 * The caller must keep DTCM powered and prevent deep sleep during transfers.
 * If PSRAM was read through a cacheable XIP aperture, invalidate that XIP cache
 * range before observing a write through XIP. Apollo5 does not flush the whole
 * D-cache as a transfer side effect.
 */
uint32_t nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif
