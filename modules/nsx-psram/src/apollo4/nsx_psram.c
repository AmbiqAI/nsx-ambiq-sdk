#include "nsx_psram.h"
#include "nsx_psram_internal.h"

#include "am_bsp.h"
#include "am_hal_mpu.h"
#include "am_devices_mspi_psram_aps25616n.h"
#include "nsx_interrupt.h"

#ifndef AM_BSP_MSPI_PSRAM_DEVICE_APS25616N
#error "nsx-psram Apollo4 currently supports BSPs that advertise AM_BSP_MSPI_PSRAM_DEVICE_APS25616N. None of the Apollo4 boards currently staged in this SDK define it; see docs/contributing or the nsx-psram README for staging a PSRAM-capable Apollo4 board."
#endif

#ifndef NSX_PSRAM_MSPI_MODULE
#define NSX_PSRAM_MSPI_MODULE 0
#endif

#ifndef NSX_PSRAM_DEVICE_SIZE_BYTES
#define NSX_PSRAM_DEVICE_SIZE_BYTES 33554432u
#endif

#ifndef NSX_PSRAM_DEVICE_CONFIG
#define NSX_PSRAM_DEVICE_CONFIG AM_HAL_MSPI_FLASH_OCTAL_DDR_CE0
#endif

#define NSX_PSRAM_CONCAT_(a, b, c) a##b##c
#define NSX_PSRAM_MSPI_SYM(prefix, n, suffix) NSX_PSRAM_CONCAT_(prefix, n, suffix)
#define NSX_PSRAM_MSPI_IRQ NSX_PSRAM_MSPI_SYM(MSPI, NSX_PSRAM_MSPI_MODULE, _IRQn)
#define NSX_PSRAM_MSPI_APERTURE NSX_PSRAM_MSPI_SYM(MSPI, NSX_PSRAM_MSPI_MODULE, _APERTURE_START_ADDR)

AM_SHARED_RW static uint32_t g_nsx_psram_dma_buffer[2560];

static void *g_nsx_psram_device_handle = NULL;
static void *g_nsx_psram_mspi_handle = NULL;

static uint32_t nsx_psram_aperture_base(uint32_t module) {
    (void)module;
    return NSX_PSRAM_MSPI_APERTURE;
}

static void nsx_psram_configure_mpu(void) {
    tMPURegion mpu_cfg = {
        .ui8RegionNumber = 7,
        .ui32BaseAddress = NSX_PSRAM_MSPI_APERTURE,
        .ui8Size = 24,
        .eAccessPermission = PRIV_RW_PUB_RW,
        .bExecuteNever = false,
        .ui16SubRegionDisable = 0,
    };

    am_hal_mpu_region_configure(&mpu_cfg, true);
    am_hal_mpu_global_configure(true, true, false);
}

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
        case 96000000u: *clock = AM_HAL_MSPI_CLK_96MHZ; break;
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
            am_devices_mspi_psram_aps25616n_ddr_disable_xip(
                g_nsx_psram_device_handle) !=
                AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            complete = false;
        }
        if (am_devices_mspi_psram_aps25616n_ddr_deinit(
                g_nsx_psram_device_handle) ==
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
    am_devices_mspi_psram_ddr_timing_config_t timing;
    info->clock_hz = cfg->clock_hz;
    info->safe_to_retry = true;
    info->timing_status = NSX_PSRAM_TIMING_NOT_RUN;
    status = nsx_psram_clock_from_hz(cfg->clock_hz, &clock);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    am_devices_mspi_psram_config_t psram_cfg = {
        .eDeviceConfig = NSX_PSRAM_DEVICE_CONFIG,
        .eClockFreq = clock,
        .pNBTxnBuf = g_nsx_psram_dma_buffer,
        .ui32NBTxnBufLength = (uint32_t)(sizeof(g_nsx_psram_dma_buffer) / sizeof(g_nsx_psram_dma_buffer[0])),
        .ui32ScramblingStartAddr = 0,
        .ui32ScramblingEndAddr = 0,
    };

    if (cfg->configure_mpu) {
        nsx_psram_configure_mpu();
    }

    info->safe_to_retry = false;
    status = am_devices_mspi_psram_aps25616n_ddr_init_timing_check(
        NSX_PSRAM_MSPI_MODULE, &psram_cfg, &timing);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        info->timing_status = NSX_PSRAM_TIMING_FAILED;
        return status;
    }
    info->safe_to_retry = true;
    info->timing_status = NSX_PSRAM_TIMING_VALID;
    info->rxdqs_delay = (uint8_t)timing.ui32Rxdqsdelay;

    g_nsx_psram_device_handle = NULL;
    g_nsx_psram_mspi_handle = NULL;
    info->safe_to_retry = false;
    status = am_devices_mspi_psram_aps25616n_ddr_init(
        NSX_PSRAM_MSPI_MODULE, &psram_cfg, &g_nsx_psram_device_handle, &g_nsx_psram_mspi_handle);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        if (g_nsx_psram_device_handle != NULL) {
            info->safe_to_retry = nsx_psram_rollback(false, false);
        }
        return status;
    }

    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = NSX_PSRAM_MSPI_IRQ,
        .handler = nsx_psram_irq_handler,
        .ctx = NULL,
        .priority = AM_IRQ_PRIORITY_DEFAULT,
        .enable = true,
    };
    status = nsx_irq_register(&irq_cfg);
    if (status != NSX_STATUS_SUCCESS) {
        info->safe_to_retry = nsx_psram_rollback(false, false);
        return status;
    }
    am_hal_interrupt_master_enable();

    status = am_devices_mspi_psram_aps25616n_apply_ddr_timing(g_nsx_psram_device_handle, &timing);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        info->timing_status = NSX_PSRAM_TIMING_FAILED;
        info->safe_to_retry = nsx_psram_rollback(true, false);
        return status;
    }

    if (cfg->enable_xip) {
        status = am_devices_mspi_psram_aps25616n_ddr_enable_xip(g_nsx_psram_device_handle);
        if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            info->safe_to_retry = nsx_psram_rollback(true, true);
            return status;
        }
    }

    info->base_address = cfg->enable_xip ? nsx_psram_aperture_base(NSX_PSRAM_MSPI_MODULE) : 0u;
    info->size_bytes = NSX_PSRAM_DEVICE_SIZE_BYTES;
    info->xip_enabled = cfg->enable_xip;
    info->timing_status = NSX_PSRAM_TIMING_VALID;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_psram_platform_read(uint32_t offset, void *buffer, uint32_t length) {
    return am_devices_mspi_psram_aps25616n_ddr_read(
        g_nsx_psram_device_handle, buffer, offset, length, true);
}

uint32_t nsx_psram_platform_write(
    uint32_t offset, const void *buffer, uint32_t length) {
    return am_devices_mspi_psram_aps25616n_ddr_write(
        g_nsx_psram_device_handle, (uint8_t *)buffer, offset, length, true);
}
