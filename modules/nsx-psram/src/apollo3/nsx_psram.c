#include "nsx_psram.h"
#include "nsx_psram_internal.h"

#include "am_bsp.h"
#include "am_devices_mspi_psram_aps6404l.h"
#include "nsx_interrupt.h"

#ifndef NSX_PSRAM_DEVICE_SIZE_BYTES
#define NSX_PSRAM_DEVICE_SIZE_BYTES 8388608u
#endif

#ifdef AM_IRQ_PRIORITY_DEFAULT
#define NSX_PSRAM_DEFAULT_IRQ_PRIORITY AM_IRQ_PRIORITY_DEFAULT
#else
#define NSX_PSRAM_DEFAULT_IRQ_PRIORITY ((1u << __NVIC_PRIO_BITS) - 1u)
#endif

#if !defined(AM_BSP_MSPI_PSRAM_INST)
#error "Apollo3p PSRAM boards must define AM_BSP_MSPI_PSRAM_INST."
#endif

#if AM_BSP_MSPI_PSRAM_INST == 0
#define NSX_PSRAM_MSPI_IRQ MSPI0_IRQn
#define NSX_PSRAM_XIP_BASE MSPI0_XIP_BASEADDR
#elif AM_BSP_MSPI_PSRAM_INST == 1
#define NSX_PSRAM_MSPI_IRQ MSPI1_IRQn
#define NSX_PSRAM_XIP_BASE MSPI1_XIP_BASEADDR
#elif AM_BSP_MSPI_PSRAM_INST == 2
#define NSX_PSRAM_MSPI_IRQ MSPI2_IRQn
#define NSX_PSRAM_XIP_BASE MSPI2_XIP_BASEADDR
#else
#error "Unsupported AM_BSP_MSPI_PSRAM_INST value."
#endif

static uint32_t g_nsx_psram_dma_buffer[2560];

static void *g_nsx_psram_device_handle = NULL;
static void *g_nsx_psram_mspi_handle = NULL;

static void nsx_psram_irq_handler(void *ctx) {
    uint32_t status;

    (void)ctx;

    if (g_nsx_psram_mspi_handle == NULL) {
        return;
    }

    am_hal_mspi_interrupt_status_get(g_nsx_psram_mspi_handle, &status, false);
    am_hal_mspi_interrupt_clear(g_nsx_psram_mspi_handle, status);
    am_hal_mspi_interrupt_service(g_nsx_psram_mspi_handle, status);
}

static uint32_t nsx_psram_clock_from_hz(
    uint32_t clock_hz, am_hal_mspi_clock_e *clock) {
    switch (clock_hz) {
        case 48000000u: *clock = AM_HAL_MSPI_CLK_48MHZ; break;
        case 24000000u: *clock = AM_HAL_MSPI_CLK_24MHZ; break;
        case 12000000u: *clock = AM_HAL_MSPI_CLK_12MHZ; break;
        default: return NSX_PSRAM_STATUS_UNSUPPORTED;
    }
    return NSX_STATUS_SUCCESS;
}

static bool nsx_psram_rollback(bool irq_registered, bool xip_may_be_enabled) {
    bool complete = true;
    bool device_deinitialized = false;

    if (irq_registered &&
        nsx_irq_unregister(NSX_PSRAM_MSPI_IRQ) != NSX_STATUS_SUCCESS) {
        complete = false;
    }
    if (g_nsx_psram_device_handle != NULL) {
        if (xip_may_be_enabled &&
            am_devices_mspi_psram_disable_xip(g_nsx_psram_device_handle) !=
                AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            complete = false;
        }
        if (am_devices_mspi_psram_deinit(g_nsx_psram_device_handle) ==
            AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            device_deinitialized = true;
        } else {
            complete = false;
        }
    }
    if (device_deinitialized) {
        g_nsx_psram_device_handle = NULL;
        g_nsx_psram_mspi_handle = NULL;
    }
    return complete;
}

uint32_t nsx_psram_platform_init(
    const nsx_psram_config_t *cfg, nsx_psram_platform_info_t *info) {
    uint32_t status;
    am_hal_mspi_clock_e clock;
    info->clock_hz = cfg->clock_hz;
    info->safe_to_retry = true;
    info->timing_status = NSX_PSRAM_TIMING_UNAVAILABLE;
    status = nsx_psram_clock_from_hz(cfg->clock_hz, &clock);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    am_devices_mspi_psram_config_t psram_cfg = {
        .eDeviceConfig = AM_HAL_MSPI_FLASH_QUAD_CE0,
        .eClockFreq = clock,
        .pNBTxnBuf = g_nsx_psram_dma_buffer,
        .ui32NBTxnBufLength = (uint32_t)(sizeof(g_nsx_psram_dma_buffer) / sizeof(g_nsx_psram_dma_buffer[0])),
        .ui32ScramblingStartAddr = 0,
        .ui32ScramblingEndAddr = 0,
    };

    /* am_devices_mspi_psram_sdr_init_timing_check() / apply_sdr_timing() are
     * NOT called here: in the vendored AmbiqSuite am_devices_mspi_psram_aps6404l.c
     * driver, those two functions are compiled only under
     * `#if defined(AM_PART_APOLLO4) || defined(AM_PART_APOLLO4B)` -- they do
     * not exist for Apollo3(p) at all (confirmed against upstream AmbiqSuite,
     * not just this vendored copy). Every real AmbiqSuite Apollo3-family
     * PSRAM reference example (mspi_psram_example, mspi_power_example,
     * cache_monitor, freertos_psram_stress, etc.) calls
     * am_devices_mspi_psram_init() directly with no timing-check step, which
     * matches Apollo3's simpler Quad-SPI (non-DDR) mode not needing DQS
     * calibration. Calling the timing-check function here previously caused
     * a link failure (undefined reference) on every real Apollo3p build. */
    g_nsx_psram_device_handle = NULL;
    g_nsx_psram_mspi_handle = NULL;
    info->safe_to_retry = false;
    status = am_devices_mspi_psram_init(
        AM_BSP_MSPI_PSRAM_INST, &psram_cfg, &g_nsx_psram_device_handle, &g_nsx_psram_mspi_handle);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        if (g_nsx_psram_device_handle != NULL) {
            info->safe_to_retry = nsx_psram_rollback(false, false);
        }
        return status;
    }

    if (cfg->enable_xip) {
        status = am_devices_mspi_psram_enable_xip(g_nsx_psram_device_handle);
        if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            info->safe_to_retry = nsx_psram_rollback(false, true);
            return status;
        }
    }

    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = NSX_PSRAM_MSPI_IRQ,
        .handler = nsx_psram_irq_handler,
        .ctx = NULL,
        .priority = NSX_PSRAM_DEFAULT_IRQ_PRIORITY,
        .enable = true,
    };
    status = nsx_irq_register(&irq_cfg);
    if (status != NSX_STATUS_SUCCESS) {
        info->safe_to_retry =
            nsx_psram_rollback(false, cfg->enable_xip);
        return status;
    }
    am_hal_interrupt_master_enable();

    info->base_address = cfg->enable_xip ? NSX_PSRAM_XIP_BASE : 0u;
    info->size_bytes = NSX_PSRAM_DEVICE_SIZE_BYTES;
    info->xip_enabled = cfg->enable_xip;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_platform_read(uint32_t offset, void *buffer, uint32_t length) {
    return am_devices_mspi_psram_read(
        g_nsx_psram_device_handle, buffer, offset, length, true);
}

uint32_t nsx_psram_platform_write(
    uint32_t offset, const void *buffer, uint32_t length) {
    return am_devices_mspi_psram_write(
        g_nsx_psram_device_handle, (uint8_t *)buffer, offset, length, true);
}
