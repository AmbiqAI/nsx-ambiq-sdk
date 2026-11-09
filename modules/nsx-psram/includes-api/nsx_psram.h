#ifndef NSX_PSRAM_H
#define NSX_PSRAM_H

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_PSRAM_V1_0_0 { .major = 1, .minor = 0, .revision = 0 }
#define NSX_PSRAM_V1_1_0 { .major = 1, .minor = 1, .revision = 0 }
#define NSX_PSRAM_OLDEST_SUPPORTED_VERSION NSX_PSRAM_V1_0_0
#define NSX_PSRAM_CURRENT_VERSION NSX_PSRAM_V1_1_0
#define NSX_PSRAM_API_ID 0xCA0012

typedef enum {
    NSX_PSRAM_STATUS_NOT_INITIALIZED = 0x1200u,
    NSX_PSRAM_STATUS_OUT_OF_RANGE,
    NSX_PSRAM_STATUS_UNSUPPORTED,
    NSX_PSRAM_STATUS_ALREADY_INITIALIZED,
    NSX_PSRAM_STATUS_FAILED_STATE,
    NSX_PSRAM_STATUS_BUSY,
    NSX_PSRAM_STATUS_TIMEOUT,
    NSX_PSRAM_STATUS_DESTINATION,
    NSX_PSRAM_STATUS_ALIGNMENT,
    NSX_PSRAM_STATUS_TRANSFER_FAILED,
} nsx_psram_status_t;

#define NSX_PSRAM_CAP_SYNC_TRANSFER (1u << 0)
#define NSX_PSRAM_CAP_XIP           (1u << 1)
#define NSX_PSRAM_CAP_TIMING_SCAN   (1u << 2)
#define NSX_PSRAM_CAP_ASYNC_READ    (1u << 3)

/* Apollo510/510B datasheet ERR014/ERR018: no DTCM DMA across a 4 KiB window. */
#define NSX_PSRAM_DTCM_DMA_BOUNDARY 4096u

extern const nsx_core_api_t nsx_psram_V1_0_0;
extern const nsx_core_api_t nsx_psram_V1_1_0;
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
    uint32_t async_depth;
    uint32_t last_transfer_status;
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
 * Blocking PSRAM-to-SRAM transfer.
 * Apollo5 caller buffers may be reused after return, including on error.
 *
 * Apollo5 requires word-aligned PSRAM offsets; caller buffers and final lengths
 * need not be word-aligned. Transfers use a private 4 KiB DTCM buffer, with CPU
 * copies to the caller only after successful DMA completion. A failure may leave
 * an already copied prefix; the remainder is not valid transferred data.
 * Other families retain their device-driver alignment, cache and error-lifetime
 * requirements; timeout does not guarantee DMA has stopped using their buffers.
 *
 * A blocking platform error enters FAILED until reset on every family. Known
 * validation refusals return before the platform operation and leave READY.
 * Retained Apollo5 DMA storage must remain powered after FAILED: do not enter
 * deep sleep or power down DTCM. Other families retain their lifetime limits.
 * Concurrent blocking calls and blocking/async contention return BUSY.
 * Apollo4 HEX DDR and Apollo5 offsets must be word aligned; Apollo5 write
 * lengths must also be word sized. Validation returns ALIGNMENT. Actual HAL
 * submission errors retain their original status.
 */
uint32_t nsx_psram_read(uint32_t offset, void *buffer, uint32_t length);

/**
 * Blocking SRAM-to-PSRAM transfer.
 * Apollo5 source buffers may be reused after return, including on error.
 *
 * Apollo5 copies through the same private DTCM storage and has the read API's
 * offset alignment, concurrency and FAILED-state restrictions. Apollo5 write
 * lengths must also be multiples of four: partial-word DMA writes can change
 * adjacent PSRAM bytes. Invalid lengths fail before submission. Errors may leave a
 * partially written PSRAM range. Neither operation cancels outstanding DMA on
 * error; its retained storage is quarantined until reset.
 *
 * The caller must keep DTCM powered and prevent deep sleep during transfers.
 * For Apollo5 cacheable XIP/DMA arenas, align and pad storage to 32-byte cache
 * lines. Before DMA writes, clean and invalidate any XIP range ever dirtied by
 * CPU writes (assume dirty if its history is unknown). Keep that alias untouched
 * until completion; invalidate its now-clean lines before reading DMA data via
 * XIP. Cleaning stale dirty lines after DMA could overwrite the new data.
 * Apollo5 am_hal_cachectrl_dcache_invalidate(NULL, true) performs whole-cache
 * clean/invalidate, as does its bus-write flush. The transfer API does neither.
 */
uint32_t nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length);

/** Caller-owned, zero-initialized; use each client from one thread at a time.
 * Counters wrap modulo 2^32. Do not edit them or reuse the client while pending.
 */
typedef struct {
    volatile uint32_t issued;
    volatile uint32_t completed;
} nsx_psram_client_t;

/** Runs once per accepted request in the MSPI IRQ: SUCCESS or TRANSFER_FAILED.
 * No PSRAM API re-entry (including queries) or blocking calls from this callback.
 * A refused/zero-length request has no callback. Callback may precede submit's
 * return: prepare application completion state before calling read_async.
 */
typedef void (*nsx_psram_done_fn)(void *ctx, uint32_t status);

/** Queue a direct PSRAM-to-DTCM DMA read (Apollo5 only).
 * Offset, buffer and length must be word aligned. The destination must be wholly
 * in canonical DTCM and one 4 KiB window. No copy or cache maintenance is added.
 * Client, destination and callback context remain alive and exclusive until
 * completion; a drain timeout does not cancel DMA or release that ownership.
 * Keep destination DTCM powered and prohibit deep sleep while active.
 *
 * Checks: client/done, state, zero length, buffer, PSRAM bounds, capability,
 * alignment, destination, contention/depth, then HAL submission. Refusals leave
 * counters/queue unchanged. Actual HAL refusal statuses are preserved.
 * At 125/250 MHz PSRAM, HAL HFRC adjustment can wait about 10 ms during
 * clock preflight with interrupts masked. Async describes data completion,
 * not an interrupt-latency bound on submission. Default PSRAM clock is 48 MHz.
 * ISR/task notification is legal only at an appropriate configured IRQ priority.
 */
uint32_t nsx_psram_read_async(nsx_psram_client_t *client, uint32_t offset,
                            void *buffer, uint32_t length,
                            nsx_psram_done_fn done, void *ctx);
/** Outstanding requests for one client, unsigned issued - completed.
 * NULL returns UINT32_MAX; it does not represent an idle client.
 */
uint32_t nsx_psram_client_pending(const nsx_psram_client_t *client);
/** Poll every microsecond; timeout_us==0 performs one check. Success means no
 * requests pending for this client, not successful data. Latch callback errors.
 * NULL returns INVALID_HANDLE. Timeout leaves requests and buffer ownership live.
 */
uint32_t nsx_psram_client_drain(const nsx_psram_client_t *client, uint32_t timeout_us);
uint32_t nsx_psram_pending(void);
/** Read Apollo5 hardware DMATIP, independently of software completion counts.
 * On success, writes a momentary activity sample. Check the returned status:
 * unsupported/uninitialized/failed state or a HAL error supplies no sample and
 * leaves *in_progress unchanged. NULL returns INVALID_HANDLE. Thread-only.
 * A false sample does not release pending buffers or authorize sleep/power-down.
 */
uint32_t nsx_psram_dma_in_progress(bool *in_progress);
/** Conservative snapshot: READY, no blocking transfer, no async requests.
 * False before/during initialization and after FAILED. Combine this check and
 * sleep entry/DTCM-bank power-down under application scheduling/IRQ exclusion.
 * This is not a persistent power lock or authorization for other peripherals.
 * All async entry points and queries are thread-only, including can_sleep.
 */
bool nsx_psram_can_sleep(void);

#ifdef __cplusplus
}
#endif

#endif
