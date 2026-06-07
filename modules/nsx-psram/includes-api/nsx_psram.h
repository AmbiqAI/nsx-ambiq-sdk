#ifndef NSX_PSRAM_H
#define NSX_PSRAM_H

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

#define NSX_PSRAM_V0_0_1 { .major = 0, .minor = 0, .revision = 1 }
#define NSX_PSRAM_OLDEST_SUPPORTED_VERSION NSX_PSRAM_V0_0_1
#define NSX_PSRAM_CURRENT_VERSION NSX_PSRAM_V0_0_1
#define NSX_PSRAM_API_ID 0xCA0012

extern const nsx_core_api_t nsx_psram_V0_0_1;
extern const nsx_core_api_t nsx_psram_oldest_supported_version;
extern const nsx_core_api_t nsx_psram_current_version;

typedef struct {
    const nsx_core_api_t *api;
    bool enable;
    bool enable_xip;
    bool configure_mpu;
    am_hal_mspi_clock_e clock_freq;
    uint32_t *nbtxn_buf;
    uint32_t nbtxn_buf_len;
    uint32_t scrambling_start_addr;
    uint32_t scrambling_end_addr;
    uint32_t base_address;
    uint32_t size_bytes;
} nsx_psram_config_t;

uint32_t nsx_psram_default_config(nsx_psram_config_t *cfg);
uint32_t nsx_psram_init(nsx_psram_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
