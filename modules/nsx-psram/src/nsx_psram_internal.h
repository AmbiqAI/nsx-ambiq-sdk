#ifndef NSX_PSRAM_INTERNAL_H
#define NSX_PSRAM_INTERNAL_H

#include "nsx_psram.h"

#ifndef NSX_PSRAM_ASYNC_DEPTH
#define NSX_PSRAM_ASYNC_DEPTH 8
#endif
#if NSX_PSRAM_ASYNC_DEPTH < 1 || NSX_PSRAM_ASYNC_DEPTH > 32
#error "NSX_PSRAM_ASYNC_DEPTH must be in [1, 32]."
#endif

#ifdef NSX_PSRAM_TEST_POINTS
void nsx_psram_test_point(void);
#define NSX_PSRAM_TEST_POINT() nsx_psram_test_point()
#else
#define NSX_PSRAM_TEST_POINT() ((void)0)
#endif

typedef struct {
    nsx_psram_client_t *client;
    nsx_psram_done_fn done;
    void *ctx;
    bool queued;
} nsx_psram_async_slot_t;

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
/* Validate known transfer constraints without enqueueing or changing state. */
uint32_t nsx_psram_platform_validate_blocking(uint32_t offset, const void *buffer, uint32_t length, bool write);
uint32_t nsx_psram_platform_write(
    uint32_t offset, const void *buffer, uint32_t length);
/* Called with interrupts masked after a platform transfer loses safe reuse. */
void nsx_psram_mark_failed(void);
uint32_t nsx_psram_platform_irq_save(void);
void nsx_psram_platform_irq_restore(uint32_t state);
void nsx_psram_platform_delay_us(uint32_t us);
uint32_t nsx_psram_platform_check_destination(const void *buffer, uint32_t length);
uint32_t nsx_psram_platform_read_async(uint32_t offset, void *buffer, uint32_t length,
                                     nsx_psram_async_slot_t *slot, bool previously_pending);
void nsx_psram_async_complete(nsx_psram_async_slot_t *slot, uint32_t status);
uint32_t nsx_psram_platform_dma_in_progress(bool *in_progress);

#endif
