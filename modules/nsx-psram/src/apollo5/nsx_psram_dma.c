#include "nsx_psram_dma.h"
#include "nsx_psram_internal.h"

#include <string.h>

#ifndef NSX_PSRAM_MSPI_MODULE
#define NSX_PSRAM_MSPI_MODULE 0
#endif
#ifndef NSX_PSRAM_ASYNC_BURST_QUALIFIED
#define NSX_PSRAM_ASYNC_BURST_QUALIFIED 0
#endif

#define NSX_PSRAM_DMA_WINDOW NSX_PSRAM_DTCM_DMA_BOUNDARY
#define NSX_PSRAM_DMA_WAIT_US 1000000u
#define NSX_PSRAM_DMA_WAITING UINT32_MAX

/* Ordinary BSS maps to DTCM in the supported linkers; validate custom layouts. */
static uint8_t g_dma_buffer[NSX_PSRAM_DMA_WINDOW]
    __attribute__((aligned(NSX_PSRAM_DMA_WINDOW)));
static volatile uint32_t g_dma_status;
static void *g_dma_handle;
static bool g_non_hfrc;
static bool g_busy;
static bool g_failed;

typedef enum {
    NSX_PSRAM_DMA_BLOCKING,
    NSX_PSRAM_DMA_ASYNC_READ,
} nsx_psram_dma_purpose_t;

#if NSX_PSRAM_ASYNC_BURST_QUALIFIED
static bool g_burst_qualified;
static uint32_t g_default_burst, g_default_threshold;

static uint32_t nsx_psram_dma_select_policy(
    nsx_psram_dma_purpose_t purpose, bool previously_pending) {
    if (!g_burst_qualified) return AM_HAL_STATUS_SUCCESS;

    MSPI0_Type *mspi = MSPIn(NSX_PSRAM_MSPI_MODULE);
    uint32_t burst = g_default_burst;
    uint32_t threshold = g_default_threshold;
    if (purpose == NSX_PSRAM_DMA_ASYNC_READ) {
        burst = (burst & ~MSPI0_DMABCOUNT_BCOUNT_Msk) |
            _VAL2FLD(MSPI0_DMABCOUNT_BCOUNT, 64u);
        threshold = (threshold & ~MSPI0_DMATHRESH_DMARXTHRESH_Msk) |
            _VAL2FLD(MSPI0_DMATHRESH_DMARXTHRESH, 15u);
    }
    if (mspi->DMABCOUNT == burst && mspi->DMATHRESH == threshold)
        return AM_HAL_STATUS_SUCCESS;
    if (previously_pending) return NSX_PSRAM_STATUS_BUSY;

    am_hal_mspi_status_t hardware = {0};
    uint32_t status = am_hal_mspi_status_get(g_dma_handle, &hardware);
    if (status != AM_HAL_STATUS_SUCCESS) return status;
    if (hardware.bTIP || hardware.ui32NumCQEntries != 0u)
        return AM_HAL_STATUS_INVALID_OPERATION;

    /* BCOUNT is shared by RX and TX. Switch only between quiescent batches. */
    mspi->DMATHRESH = threshold;
    mspi->DMABCOUNT = burst;
    __DSB();
    if (mspi->DMABCOUNT != burst || mspi->DMATHRESH != threshold)
        return AM_HAL_STATUS_FAIL;
    return AM_HAL_STATUS_SUCCESS;
}
#endif

uint32_t nsx_psram_dma_check_destination(const void *buffer, uint32_t length) {
    uintptr_t address = (uintptr_t)buffer;
    if (length == 0u || length > NSX_PSRAM_DMA_WINDOW || length > DTCM_MAX_SIZE ||
        address < DTCM_BASEADDR ||
        address - DTCM_BASEADDR > DTCM_MAX_SIZE - length ||
        (address - DTCM_BASEADDR) % NSX_PSRAM_DMA_WINDOW > NSX_PSRAM_DMA_WINDOW - length) {
        return NSX_PSRAM_STATUS_DESTINATION;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_dma_validate_storage(void) {
    uintptr_t address = (uintptr_t)g_dma_buffer;
    if (address < DTCM_BASEADDR ||
        address - DTCM_BASEADDR > DTCM_MAX_SIZE - sizeof(g_dma_buffer) ||
        (address - DTCM_BASEADDR) % NSX_PSRAM_DMA_WINDOW != 0u) {
        return AM_HAL_STATUS_INVALID_ARG;
    }
    return AM_HAL_STATUS_SUCCESS;
}

void nsx_psram_dma_configure(void *handle, uint32_t clock_hz) {
    g_dma_handle = handle;
    g_non_hfrc = clock_hz == 125000000u || clock_hz == 250000000u;
#if NSX_PSRAM_ASYNC_BURST_QUALIFIED
    g_burst_qualified = clock_hz == 48000000u || clock_hz == 96000000u;
    if (g_burst_qualified) {
        g_default_burst = MSPIn(NSX_PSRAM_MSPI_MODULE)->DMABCOUNT;
        g_default_threshold = MSPIn(NSX_PSRAM_MSPI_MODULE)->DMATHRESH;
    }
#endif
}

static uint32_t nsx_psram_dma_enqueue(am_hal_mspi_dma_transfer_t *transfer,
                                    bool previously_pending,
                                    am_hal_mspi_callback_t done, void *ctx,
                                    nsx_psram_dma_purpose_t purpose) {
    am_hal_clkmgr_user_id_e user =
        (am_hal_clkmgr_user_id_e)(AM_HAL_CLKMGR_USER_ID_MSPI0 + NSX_PSRAM_MSPI_MODULE);
    /* The HAL requests this clock after posting the CQ entry. Preflight it so
     * a clock error cannot leave retained callback state on a refused request. */
    uint32_t status = am_hal_clkmgr_clock_request(AM_HAL_CLKMGR_CLK_ID_HFRC, user);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }
#if NSX_PSRAM_ASYNC_BURST_QUALIFIED
    status = nsx_psram_dma_select_policy(purpose, previously_pending);
#else
    (void)purpose;
#endif
    if (status == AM_HAL_STATUS_SUCCESS) {
        __DMB();
        status = am_hal_mspi_nonblocking_transfer(
            g_dma_handle, transfer, AM_HAL_MSPI_TRANS_DMA, done, ctx);
    }
    if (status != AM_HAL_STATUS_SUCCESS && g_non_hfrc && !previously_pending) {
        /* HFRC ownership is a user bit, not a refcount. Successful requests
         * leave its release to HAL IRQ cleanup, after the callback. */
        (void)am_hal_clkmgr_clock_release(AM_HAL_CLKMGR_CLK_ID_HFRC, user);
    }
    return status;
}

uint32_t nsx_psram_dma_submit(am_hal_mspi_dma_transfer_t *transfer,
                            bool previously_pending,
                            am_hal_mspi_callback_t done, void *ctx) {
    return nsx_psram_dma_enqueue(transfer, previously_pending, done, ctx,
                                NSX_PSRAM_DMA_ASYNC_READ);
}

static void nsx_psram_dma_done(void *ctx, uint32_t status) {
    (void)ctx;
    __DMB();
    g_dma_status = status;
}

static uint32_t nsx_psram_dma_transfer(
    uint32_t offset, void *read_buffer, const void *write_buffer, uint32_t length) {
    bool write = write_buffer != NULL;
    uint32_t irq = am_hal_interrupt_master_disable();
    uint32_t status;
    if (g_failed) {
        am_hal_interrupt_master_set(irq);
        return NSX_PSRAM_STATUS_FAILED_STATE;
    }
    if (g_busy) {
        am_hal_interrupt_master_set(irq);
        return AM_HAL_STATUS_IN_USE;
    }
    if ((offset & 3u) != 0u || (write && (length & 3u) != 0u)) {
        am_hal_interrupt_master_set(irq);
        return AM_HAL_STATUS_INVALID_OPERATION;
    }
    g_busy = true;
    am_hal_interrupt_master_set(irq);

    uint32_t remaining_wait = NSX_PSRAM_DMA_WAIT_US;
    uint32_t copied = 0u;
    status = AM_HAL_STATUS_SUCCESS;
    while (copied < length) {
        uint32_t count = length - copied;
        if (count > sizeof(g_dma_buffer)) {
            count = sizeof(g_dma_buffer);
        }
        if (write) {
            memcpy(g_dma_buffer, (const uint8_t *)write_buffer + copied, count);
        }
        am_hal_mspi_dma_transfer_t transfer = {
            .ui8Priority = 1,
            .eDirection = write ? AM_HAL_MSPI_TX : AM_HAL_MSPI_RX,
            .ui32TransferCount = count,
            .ui32DeviceAddress = offset + copied,
            .ui32SRAMAddress = (uint32_t)(uintptr_t)g_dma_buffer,
        };
        irq = am_hal_interrupt_master_disable();
        g_dma_status = NSX_PSRAM_DMA_WAITING;
        status = nsx_psram_dma_enqueue(&transfer, false, nsx_psram_dma_done, NULL,
                                      NSX_PSRAM_DMA_BLOCKING);
        am_hal_interrupt_master_set(irq);
        if (status != AM_HAL_STATUS_SUCCESS) {
            break;
        }
        while (g_dma_status == NSX_PSRAM_DMA_WAITING && remaining_wait != 0u) {
            am_hal_delay_us(1);
            --remaining_wait;
        }
        status = g_dma_status;
        if (status == NSX_PSRAM_DMA_WAITING) {
            status = AM_HAL_STATUS_TIMEOUT;
        }
        if (status != AM_HAL_STATUS_SUCCESS) {
            break;
        }
        __DMB();
        if (!write) {
            memcpy((uint8_t *)read_buffer + copied, g_dma_buffer, count);
        }
        copied += count;
    }

    irq = am_hal_interrupt_master_disable();
    if (status != AM_HAL_STATUS_SUCCESS) {
        /* DMA may still own the persistent buffer. Never recycle it or the
         * completion state after an error, even if completion arrives later. */
        g_failed = true;
        nsx_psram_mark_failed();
    }
    g_busy = false;
    am_hal_interrupt_master_set(irq);
    return status;
}

uint32_t nsx_psram_dma_read(uint32_t offset, void *buffer, uint32_t length) {
    return nsx_psram_dma_transfer(offset, buffer, NULL, length);
}

uint32_t nsx_psram_dma_write(uint32_t offset, const void *buffer, uint32_t length) {
    return nsx_psram_dma_transfer(offset, NULL, buffer, length);
}
