#include <assert.h>
#include <string.h>
#include "nsx_psram_dma.h"
#include "nsx_psram_internal.h"

static unsigned irq, requests, releases, submitted, poisoned, barriers;
static uint32_t clock_status, submit_status, completion_status;
static unsigned error_at, hold_at;
static bool hold, nested;
static uint8_t psram[16384];
static am_hal_mspi_dma_transfer_t queued;
static am_hal_mspi_callback_t callback;
static void *callback_ctx;

uint32_t am_hal_interrupt_master_disable(void) {
    uint32_t old = irq; irq = 1; return old;
}
void am_hal_interrupt_master_set(uint32_t state) { irq = state; }
void test_dmb(void) { ++barriers; }
void nsx_psram_mark_failed(void) { assert(irq); ++poisoned; }
uint32_t am_hal_clkmgr_clock_request(unsigned clock, am_hal_clkmgr_user_id_e user) {
    assert(irq && clock == AM_HAL_CLKMGR_CLK_ID_HFRC);
    assert(user == AM_HAL_CLKMGR_USER_ID_MSPI0);
    ++requests; return clock_status;
}
uint32_t am_hal_clkmgr_clock_release(unsigned clock, am_hal_clkmgr_user_id_e user) {
    assert(irq && clock == AM_HAL_CLKMGR_CLK_ID_HFRC);
    assert(user == AM_HAL_CLKMGR_USER_ID_MSPI0);
    ++releases; return 0;
}
uint32_t am_hal_mspi_nonblocking_transfer(void *handle, void *transfer, unsigned mode,
                                        am_hal_mspi_callback_t done, void *ctx) {
    assert(irq && handle == psram && mode == AM_HAL_MSPI_TRANS_DMA);
    assert(callback == NULL);
    ++submitted;
    if (submit_status) { return submit_status; }
    queued = *(am_hal_mspi_dma_transfer_t *)transfer;
    assert(queued.ui8Priority == 1 && !queued.ui32PauseCondition && !queued.ui32StatusSetClr);
    assert(queued.ui32TransferCount <= 4096 && !(queued.ui32DeviceAddress & 3));
    assert(!(queued.ui32SRAMAddress & 4095));
    assert(queued.ui32DeviceAddress + queued.ui32TransferCount <= sizeof(psram));
    callback = done; callback_ctx = ctx;
    return 0;
}
static void complete(void) {
    assert(callback);
    void *buffer = (void *)(uintptr_t)queued.ui32SRAMAddress;
    if (queued.eDirection == AM_HAL_MSPI_RX) {
        memcpy(buffer, psram + queued.ui32DeviceAddress, queued.ui32TransferCount);
    } else {
        memcpy(psram + queued.ui32DeviceAddress, buffer, queued.ui32TransferCount);
    }
    am_hal_mspi_callback_t done = callback;
    callback = NULL;
    done(callback_ctx, error_at == submitted ? 123u : completion_status);
}
void am_hal_delay_us(uint32_t us) {
    assert(us == 1);
    if (nested) {
        nested = false;
        uint8_t data[4];
        assert(nsx_psram_dma_read(0, data, sizeof(data)) == AM_HAL_STATUS_IN_USE);
        assert(nsx_psram_dma_write(0, data, sizeof(data)) == AM_HAL_STATUS_IN_USE);
    }
    if (!hold && hold_at != submitted && !irq && callback) { complete(); }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    uint8_t data[8200];
    memset(data, 0x5a, sizeof(data));
    for (unsigned i = 0; i < sizeof(psram); ++i) { psram[i] = (uint8_t)(i * 17 + 9); }
    nsx_psram_dma_configure(psram, 48000000u);
    if (!strcmp(argv[1], "placement")) {
#ifdef EXPECT_BAD_PLACEMENT
        assert(nsx_psram_dma_validate_storage() == AM_HAL_STATUS_INVALID_ARG);
#else
        assert(nsx_psram_dma_validate_storage() == 0);
#endif
        return 0;
    }
    if (!strcmp(argv[1], "success")) {
        nested = true;
        assert(nsx_psram_dma_read(4, data + 1, 8195) == 0);
        assert(!memcmp(data + 1, psram + 4, 8195));
        assert(data[0] == 0x5a && data[8196] == 0x5a);
        assert(submitted == 3 && !irq && !poisoned);
        memset(data + 1, 0xa7, 8195);
        assert(nsx_psram_dma_write(4, data + 1, 8195) == 0);
        assert(!memcmp(data + 1, psram + 4, 8195));
        assert(psram[3] == (uint8_t)(3 * 17 + 9));
        assert(psram[8199] == (uint8_t)(8199 * 17 + 9));
        assert(submitted == 6 && barriers >= 18);
        return 0;
    }
    if (!strcmp(argv[1], "alignment")) {
        assert(nsx_psram_dma_read(1, data, 4) == AM_HAL_STATUS_INVALID_OPERATION);
        assert(nsx_psram_dma_write(3, data, 4) == AM_HAL_STATUS_INVALID_OPERATION);
        assert(!submitted && !requests && !poisoned && !irq);
        assert(nsx_psram_dma_read(0, data, 3) == 0);
        return 0;
    }
    if (!strcmp(argv[1], "clocks")) {
        am_hal_mspi_dma_transfer_t transfer = {0};
        irq = 1;
        clock_status = 88;
        assert(nsx_psram_dma_submit(&transfer, false, NULL, NULL) == 88);
        assert(!submitted && !releases);
        clock_status = 0; submit_status = 77;
        assert(nsx_psram_dma_submit(&transfer, false, NULL, NULL) == 77);
        assert(!releases);
        nsx_psram_dma_configure(psram, 125000000u);
        assert(nsx_psram_dma_submit(&transfer, false, NULL, NULL) == 77);
        assert(releases == 1);
        assert(nsx_psram_dma_submit(&transfer, true, NULL, NULL) == 77);
        assert(releases == 1);
        nsx_psram_dma_configure(psram, 250000000u);
        assert(nsx_psram_dma_submit(&transfer, false, NULL, NULL) == 77);
        assert(releases == 2);
        return 0;
    }
    if (!strcmp(argv[1], "later-read-error") || !strcmp(argv[1], "later-write-error")) {
        error_at = 2;
        bool write = !strcmp(argv[1], "later-write-error");
        uint32_t status = write ? nsx_psram_dma_write(0, data, 8195)
                                : nsx_psram_dma_read(0, data, 8195);
        assert(status == 123 && submitted == 2 && poisoned == 1);
        if (!write) {
            assert(!memcmp(data, psram, 4096));
            for (unsigned i = 4096; i < sizeof(data); ++i) { assert(data[i] == 0x5a); }
        } else {
            for (unsigned i = 0; i < 4096; ++i) { assert(psram[i] == 0x5a); }
            /* The failed chunk may have reached PSRAM; the third never did. */
            assert(psram[8192] == (uint8_t)(8192 * 17 + 9));
        }
        assert(nsx_psram_dma_read(0, data, 4) == NSX_PSRAM_STATUS_FAILED_STATE);
        return 0;
    }
    if (!strcmp(argv[1], "later-timeout")) {
        hold_at = 2;
        assert(nsx_psram_dma_read(0, data, 8195) == AM_HAL_STATUS_TIMEOUT);
        assert(!memcmp(data, psram, 4096));
        for (unsigned i = 4096; i < sizeof(data); ++i) { assert(data[i] == 0x5a); }
        memset(data, 0xee, sizeof(data));
        complete();
        for (unsigned i = 0; i < sizeof(data); ++i) { assert(data[i] == 0xee); }
        assert(poisoned == 1 && submitted == 2);
        assert(nsx_psram_dma_write(0, data, 4) == NSX_PSRAM_STATUS_FAILED_STATE);
        return 0;
    }
    if (!strcmp(argv[1], "error")) {
        completion_status = 123;
        assert(nsx_psram_dma_read(0, data, 4) == 123);
        assert(data[0] == 0x5a && poisoned == 1);
    } else {
        hold = true;
        bool write = !strcmp(argv[1], "timeout-write");
        if (!strcmp(argv[1], "masked")) { irq = 1; }
        unsigned old_irq = irq;
        uint32_t status = write ? nsx_psram_dma_write(0, data, 4)
                                : nsx_psram_dma_read(0, data, 4);
        assert(status == AM_HAL_STATUS_TIMEOUT && irq == old_irq);
        assert(poisoned == 1 && callback);
        memset(data, 0xee, sizeof(data));
        complete(); /* Late DMA and callback after caller has reused its buffer. */
        for (unsigned i = 0; i < sizeof(data); ++i) { assert(data[i] == 0xee); }
        if (write) { assert(psram[0] == 0x5a); }
    }
    assert(nsx_psram_dma_read(0, data, 4) == NSX_PSRAM_STATUS_FAILED_STATE);
    assert(nsx_psram_dma_write(0, data, 4) == NSX_PSRAM_STATUS_FAILED_STATE);
    assert(submitted == 1);
    return 0;
}
