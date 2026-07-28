#ifndef NSX_PSRAM_INTERNAL_H
#define NSX_PSRAM_INTERNAL_H

#include "nsx_psram.h"

typedef struct {
    uint32_t base_address;
    uint32_t size_bytes;
    uint32_t clock_hz;
    bool safe_to_retry;
    bool xip_enabled;
    nsx_psram_timing_status_t timing_status;
    uint8_t rxdqs_delay;
} nsx_psram_platform_info_t;

uint32_t nsx_psram_platform_init(
    const nsx_psram_config_t *cfg, nsx_psram_platform_info_t *info);
uint32_t nsx_psram_platform_read(uint32_t offset, void *buffer, uint32_t length);
uint32_t nsx_psram_platform_write(
    uint32_t offset, const void *buffer, uint32_t length);

#endif
