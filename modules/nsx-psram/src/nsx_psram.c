#include "nsx_psram.h"
#include "nsx_psram_internal.h"

const nsx_core_api_t nsx_psram_V1_0_0 = {.apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_V1_0_0};
const nsx_core_api_t nsx_psram_V1_1_0 = {.apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_V1_1_0};
const nsx_core_api_t nsx_psram_oldest_supported_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_OLDEST_SUPPORTED_VERSION};
const nsx_core_api_t nsx_psram_current_version = {
    .apiId = NSX_PSRAM_API_ID, .version = NSX_PSRAM_CURRENT_VERSION};

#ifndef NSX_PSRAM_PLATFORM_CAPABILITIES
#define NSX_PSRAM_PLATFORM_CAPABILITIES NSX_PSRAM_CAP_SYNC_TRANSFER
#endif

static nsx_psram_info_t g_nsx_psram_info = {
    .capabilities = NSX_PSRAM_PLATFORM_CAPABILITIES,
    .state = NSX_PSRAM_STATE_UNINITIALIZED,
    .timing_status = NSX_PSRAM_TIMING_UNAVAILABLE,
};
static nsx_psram_async_slot_t g_async_slots[NSX_PSRAM_ASYNC_DEPTH];
static uint32_t g_async_issued, g_async_completed;
static bool g_blocking_active;

void nsx_psram_mark_failed(void) {
    g_nsx_psram_info.state = NSX_PSRAM_STATE_FAILED;
}

static uint32_t nsx_psram_validate_transfer(
    uint32_t offset, const void *buffer, uint32_t length) {
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_FAILED) {
        return NSX_PSRAM_STATUS_FAILED_STATE;
    }
    if (g_nsx_psram_info.state != NSX_PSRAM_STATE_READY) {
        return NSX_PSRAM_STATUS_NOT_INITIALIZED;
    }
    if (length == 0u) {
        return NSX_STATUS_SUCCESS;
    }
    if (buffer == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (offset >= g_nsx_psram_info.size_bytes ||
        length > g_nsx_psram_info.size_bytes - offset) {
        return NSX_PSRAM_STATUS_OUT_OF_RANGE;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_default_config(nsx_psram_config_t *cfg) {
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    *cfg = (nsx_psram_config_t){
        .api = &nsx_psram_current_version,
        .clock_hz = 48000000u,
        .enable_xip = true,
        .configure_mpu = true,
    };

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_init(const nsx_psram_config_t *cfg) {
    nsx_psram_platform_info_t platform_info = {
        .safe_to_retry = true,
        .timing_status = NSX_PSRAM_TIMING_UNAVAILABLE,
    };
    uint32_t status;

#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_psram_oldest_supported_version,
                          &nsx_psram_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg == NULL || cfg->clock_hz == 0u) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_READY) {
        return NSX_PSRAM_STATUS_ALREADY_INITIALIZED;
    }
    if (g_nsx_psram_info.state == NSX_PSRAM_STATE_FAILED) {
        return NSX_PSRAM_STATUS_FAILED_STATE;
    }

    status = nsx_psram_platform_init(cfg, &platform_info);
    uint32_t irq = nsx_psram_platform_irq_save();
    g_nsx_psram_info = (nsx_psram_info_t){
        .base_address = platform_info.base_address,
        .size_bytes = platform_info.size_bytes,
        .configured_clock_hz = platform_info.clock_hz,
        .capabilities = NSX_PSRAM_PLATFORM_CAPABILITIES,
        .last_init_status = status,
        .state = status == NSX_STATUS_SUCCESS
            ? NSX_PSRAM_STATE_READY
            : (platform_info.safe_to_retry
                ? NSX_PSRAM_STATE_UNINITIALIZED
                : NSX_PSRAM_STATE_FAILED),
        .xip_enabled = platform_info.xip_enabled,
        .timing_status = platform_info.timing_status,
        .rxdqs_delay = platform_info.rxdqs_delay,
        .async_depth = (NSX_PSRAM_PLATFORM_CAPABILITIES & NSX_PSRAM_CAP_ASYNC_READ)
            ? NSX_PSRAM_ASYNC_DEPTH : 0u,
        .last_transfer_status = 0u,
    };
    nsx_psram_platform_irq_restore(irq);
    return status;
}

uint32_t nsx_psram_get_info(nsx_psram_info_t *info) {
    if (info == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    uint32_t irq = nsx_psram_platform_irq_save();
    *info = g_nsx_psram_info;
    nsx_psram_platform_irq_restore(irq);
    return NSX_STATUS_SUCCESS;
}

static uint32_t nsx_psram_blocking(uint32_t offset, void *read_buffer,
                                  const void *write_buffer, uint32_t length,
                                  bool write) {
    const void *buffer = write ? write_buffer : read_buffer;
    NSX_PSRAM_TEST_POINT();
    uint32_t irq = nsx_psram_platform_irq_save();
    uint32_t status = nsx_psram_validate_transfer(offset, buffer, length);
    if (status == NSX_STATUS_SUCCESS && length != 0u) {
        status = nsx_psram_platform_validate_blocking(offset, buffer, length, write);
    }
    if (status == NSX_STATUS_SUCCESS && length != 0u &&
        (g_blocking_active || g_async_issued != g_async_completed)) {
        status = NSX_PSRAM_STATUS_BUSY;
    }
    bool entered = status == NSX_STATUS_SUCCESS && length != 0u;
    if (entered) g_blocking_active = true;
    NSX_PSRAM_TEST_POINT();
    nsx_psram_platform_irq_restore(irq);
    NSX_PSRAM_TEST_POINT();
    if (!entered) return status;

    status = write ? nsx_psram_platform_write(offset, write_buffer, length)
                   : nsx_psram_platform_read(offset, read_buffer, length);
    irq = nsx_psram_platform_irq_save();
    if (status != NSX_STATUS_SUCCESS) nsx_psram_mark_failed();
    g_blocking_active = false;
    nsx_psram_platform_irq_restore(irq);
    NSX_PSRAM_TEST_POINT();
    return status;
}

uint32_t nsx_psram_read(uint32_t offset, void *buffer, uint32_t length) {
    return nsx_psram_blocking(offset, buffer, NULL, length, false);
}

uint32_t nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length) {
    return nsx_psram_blocking(offset, NULL, buffer, length, true);
}

uint32_t nsx_psram_read_async(nsx_psram_client_t *client, uint32_t offset,
                            void *buffer, uint32_t length,
                            nsx_psram_done_fn done, void *ctx) {
    if (client == NULL || done == NULL) return NSX_STATUS_INVALID_HANDLE;
    NSX_PSRAM_TEST_POINT();
    uint32_t irq = nsx_psram_platform_irq_save();
    uint32_t status = nsx_psram_validate_transfer(offset, buffer, length);
    if (status == NSX_STATUS_SUCCESS && length != 0u) {
        if (!(g_nsx_psram_info.capabilities & NSX_PSRAM_CAP_ASYNC_READ)) {
            status = NSX_PSRAM_STATUS_UNSUPPORTED;
        } else if ((offset | (uintptr_t)buffer | length) & 3u) {
            status = NSX_PSRAM_STATUS_ALIGNMENT;
        } else {
            status = nsx_psram_platform_check_destination(buffer, length);
        }
    }
    nsx_psram_async_slot_t *slot = NULL;
    if (status == NSX_STATUS_SUCCESS && length != 0u) {
        if (!g_blocking_active) {
            for (uint32_t i = 0u; i < NSX_PSRAM_ASYNC_DEPTH; ++i) {
                if (!g_async_slots[i].queued) { slot = &g_async_slots[i]; break; }
            }
        }
        if (slot == NULL) status = NSX_PSRAM_STATUS_BUSY;
    }
    if (slot != NULL) {
        bool previously_pending = g_async_issued != g_async_completed;
        *slot = (nsx_psram_async_slot_t){.client=client, .done=done, .ctx=ctx, .queued=true};
        ++client->issued;
        ++g_async_issued;
        NSX_PSRAM_TEST_POINT();
        status = nsx_psram_platform_read_async(offset, buffer, length, slot, previously_pending);
        if (status != NSX_STATUS_SUCCESS) {
            --client->issued;
            --g_async_issued;
            slot->queued = false;
        }
    }
    NSX_PSRAM_TEST_POINT();
    nsx_psram_platform_irq_restore(irq);
    NSX_PSRAM_TEST_POINT();
    return status;
}

void nsx_psram_async_complete(nsx_psram_async_slot_t *slot, uint32_t status) {
    uint32_t irq = nsx_psram_platform_irq_save();
    nsx_psram_client_t *client = slot->client;
    nsx_psram_done_fn done = slot->done;
    void *ctx = slot->ctx;
    slot->queued = false;
    ++client->completed;
    ++g_async_completed;
    if (status != NSX_STATUS_SUCCESS) {
        g_nsx_psram_info.last_transfer_status = status;
    }
    nsx_psram_platform_irq_restore(irq);
    done(ctx, status == NSX_STATUS_SUCCESS ? NSX_STATUS_SUCCESS
                                         : NSX_PSRAM_STATUS_TRANSFER_FAILED);
}

uint32_t nsx_psram_client_pending(const nsx_psram_client_t *client) {
    if (client == NULL) return UINT32_MAX;
    NSX_PSRAM_TEST_POINT();
    uint32_t irq = nsx_psram_platform_irq_save();
    uint32_t pending = client->issued - client->completed;
    nsx_psram_platform_irq_restore(irq);
    NSX_PSRAM_TEST_POINT();
    return pending;
}

uint32_t nsx_psram_client_drain(const nsx_psram_client_t *client, uint32_t timeout_us) {
    if (client == NULL) return NSX_STATUS_INVALID_HANDLE;
    for (;;) {
        if (nsx_psram_client_pending(client) == 0u) return NSX_STATUS_SUCCESS;
        if (timeout_us == 0u) return NSX_PSRAM_STATUS_TIMEOUT;
        nsx_psram_platform_delay_us(1u);
        --timeout_us;
    }
}

uint32_t nsx_psram_pending(void) {
    NSX_PSRAM_TEST_POINT();
    uint32_t irq = nsx_psram_platform_irq_save();
    uint32_t pending = g_async_issued - g_async_completed;
    nsx_psram_platform_irq_restore(irq);
    NSX_PSRAM_TEST_POINT();
    return pending;
}

uint32_t nsx_psram_dma_in_progress(bool *in_progress) {
    if (in_progress == NULL) return NSX_STATUS_INVALID_HANDLE;
    uint32_t irq = nsx_psram_platform_irq_save();
    uint32_t status = nsx_psram_validate_transfer(0u, NULL, 0u);
    if (status == NSX_STATUS_SUCCESS) {
        status = (g_nsx_psram_info.capabilities & NSX_PSRAM_CAP_ASYNC_READ)
            ? nsx_psram_platform_dma_in_progress(in_progress)
            : NSX_PSRAM_STATUS_UNSUPPORTED;
    }
    nsx_psram_platform_irq_restore(irq);
    return status;
}

bool nsx_psram_can_sleep(void) {
    uint32_t irq = nsx_psram_platform_irq_save();
    bool result = g_nsx_psram_info.state == NSX_PSRAM_STATE_READY &&
                  !g_blocking_active && g_async_issued == g_async_completed;
    nsx_psram_platform_irq_restore(irq);
    return result;
}
