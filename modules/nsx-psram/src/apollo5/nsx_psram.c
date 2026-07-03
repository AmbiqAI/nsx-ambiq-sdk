#include "nsx_psram.h"

#include "am_bsp.h"
#include "am_devices_mspi_psram_aps25616n.h"
#include "nsx_interrupt.h"

#ifndef NSX_PSRAM_MSPI_MODULE
#define NSX_PSRAM_MSPI_MODULE 0
#endif

#ifndef NSX_PSRAM_DEVICE_SIZE_BYTES
#define NSX_PSRAM_DEVICE_SIZE_BYTES 33554432u
#endif

#ifndef AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA
#error "nsx-psram R5 currently supports BSPs that advertise AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA."
#endif
/* NOTE: despite the BSP macro name (a holdover from the original, incorrect
 * assumption that the populated PSRAM was the Octal APS25616BA part), the
 * actual chip on real Apollo510(B) EVBs is a Hex (x16) DDR part
 * (AP Memory APS512XXN / APS512XXB). This file drives it with the Hex
 * driver (am_devices_mspi_psram_aps25616n) accordingly; see
 * nsx_psram_platform_init() below. */

/*
 * The MSPI instance is fixed at compile time (NSX_PSRAM_MSPI_MODULE), so the
 * IRQ and aperture are selected by macro. This avoids referencing MSPI symbols
 * (e.g. MSPI3_IRQn) that do not exist on every supported SoC family.
 */
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
    am_hal_mpu_region_config_t mpu_cfg = {
        .ui32RegionNumber = 6,
        .ui32BaseAddress = (uint32_t)g_nsx_psram_dma_buffer,
        .eShareable = NON_SHARE,
        .eAccessPermission = RW_NONPRIV,
        .bExecuteNever = true,
        .ui32LimitAddress = (uint32_t)g_nsx_psram_dma_buffer + sizeof(g_nsx_psram_dma_buffer) - 1,
        .ui32AttrIndex = 0,
        .bEnable = true,
    };
    am_hal_mpu_attr_t mpu_attr = {
        .ui8AttrIndex = 0,
        .bNormalMem = true,
        .sOuterAttr = {
            .bNonTransient = false,
            .bWriteBack = true,
            .bReadAllocate = false,
            .bWriteAllocate = false,
        },
        .sInnerAttr = {
            .bNonTransient = false,
            .bWriteBack = true,
            .bReadAllocate = false,
            .bWriteAllocate = false,
        },
        .eDeviceAttr = 0,
    };

    am_hal_mpu_attr_configure(&mpu_attr, 1);
    am_hal_mpu_region_clear();
    am_hal_mpu_region_configure(&mpu_cfg, 1);
    am_hal_cachectrl_dcache_invalidate(NULL, true);
    am_hal_mpu_enable(true, true);
}

//
// Registered with nsx-interrupt for the configured MSPI line. The raw vector
// glue (am_mspiN_isr) lives in nsx-interrupt; this module only owns the
// device-level servicing.
//
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

uint32_t nsx_psram_platform_init(nsx_psram_config_t *cfg) {
    uint32_t status;
    am_devices_mspi_psram_config_t psram_cfg = {
        /* The populated PSRAM on Apollo510(B) EVBs (AP Memory APS512XXN/APS512XXB)
         * is a Hex (x16) DDR part, not Octal. There is no BSP macro for Hex DDR
         * chip-select mode (only AM_BSP_MSPI_PSRAM_MODULE_OCTAL_DDR_CE exists,
         * left over from the wrong Octal/BA device assumption), so the Hex CE0
         * mode is specified directly here to match the am_devices_mspi_psram_aps25616n
         * (Hex) driver used below. */
        .eDeviceConfig = AM_HAL_MSPI_FLASH_HEX_DDR_CE0,
        .eClockFreq = cfg->clock_freq,
        .pNBTxnBuf = cfg->nbtxn_buf != NULL ? cfg->nbtxn_buf : g_nsx_psram_dma_buffer,
        .ui32NBTxnBufLength = cfg->nbtxn_buf_len != 0
            ? cfg->nbtxn_buf_len
            : (uint32_t)(sizeof(g_nsx_psram_dma_buffer) / sizeof(g_nsx_psram_dma_buffer[0])),
        .ui32ScramblingStartAddr = cfg->scrambling_start_addr,
        .ui32ScramblingEndAddr = cfg->scrambling_end_addr,
    };

    if (cfg->configure_mpu) {
        nsx_psram_configure_mpu();
    }

    /* NOTE: am_devices_mspi_psram_aps25616n_ddr_init_timing_check() /
     * apply_ddr_timing() are deliberately NOT called here. On real
     * apollo510(b)_evb hardware, the automatic RXDQSDELAY scan consistently
     * reports "no valid setting" across its entire sweep, yet
     * am_devices_mspi_psram_aps25616n_ddr_init() itself succeeds and -
     * crucially - direct CPU memory-mapped access through the XIP aperture
     * (the pattern this module's base_address/enable_xip contract expects
     * callers to use) reads/writes data cleanly with zero mismatches. The
     * periodic data corruption that motivated the timing-check step only
     * ever showed up when driving the device through the DMA-based bulk
     * am_devices_mspi_psram_aps25616n_ddr_read()/ddr_write() transfer APIs
     * (which this module does not use), not through XIP-mapped access. */
    status = am_devices_mspi_psram_aps25616n_ddr_init(
        NSX_PSRAM_MSPI_MODULE, &psram_cfg, &g_nsx_psram_device_handle, &g_nsx_psram_mspi_handle);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
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
        return status;
    }
    am_hal_interrupt_master_enable();

    if (cfg->enable_xip) {
        status = am_devices_mspi_psram_aps25616n_ddr_enable_xip(g_nsx_psram_device_handle);
        if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            return status;
        }
    }

    cfg->base_address = nsx_psram_aperture_base(NSX_PSRAM_MSPI_MODULE);
    cfg->size_bytes = NSX_PSRAM_DEVICE_SIZE_BYTES;
    return NSX_STATUS_SUCCESS;
}
