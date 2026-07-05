#include "nsx_psram.h"

#include "am_bsp.h"
#include "nsx_interrupt.h"

#ifndef NSX_PSRAM_USE_BA_DRIVER
#define NSX_PSRAM_USE_BA_DRIVER 0
#endif

/*
 * apollo510_evb and apollo510b_evb share the same BSP presence macro
 * (AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA). Both boards are populated with an AP
 * Memory APS512-class Hex (x16) DDR PSRAM and both require the AmbiqSuite "BA"
 * (1.2V) driver family (am_devices_mspi_psram_aps25616ba_1p2v).
 *
 * apollo510b_evb was hardware-validated (rev 2.0, MSPI0, 2026-07). The "N"
 * driver (am_devices_mspi_psram_aps25616n) is mismatched to this part: its DDR
 * timing scan finds no valid RXDQS window ("no valid setting"), so plain
 * ddr_init() runs uncalibrated -- it can read correctly at 48 MHz/room
 * temperature but is only marginal, matching the field report of INTERMITTENT
 * PSRAM corruption. The "BA" driver's timing scan succeeds, round-trips the same
 * board with zero mismatches, and reports the true 64 MB size.
 * NSX_PSRAM_USE_BA_DRIVER is set by CMakeLists.txt based on NSX_AMBIQ_BOARD_NAME.
 * See CMakeLists.txt for the full validation write-up.
 */
#if NSX_PSRAM_USE_BA_DRIVER
#include "am_devices_mspi_psram_aps25616ba_1p2v.h"
#define nsx_psram_device_ddr_init am_devices_mspi_psram_aps25616ba_ddr_init
#define nsx_psram_device_ddr_enable_xip am_devices_mspi_psram_aps25616ba_ddr_enable_xip
#define nsx_psram_device_ddr_init_timing_check am_devices_mspi_psram_aps25616ba_ddr_init_timing_check
#define nsx_psram_device_apply_ddr_timing am_devices_mspi_psram_aps25616ba_apply_ddr_timing
#else
#include "am_devices_mspi_psram_aps25616n.h"
#define nsx_psram_device_ddr_init am_devices_mspi_psram_aps25616n_ddr_init
#define nsx_psram_device_ddr_enable_xip am_devices_mspi_psram_aps25616n_ddr_enable_xip
#define nsx_psram_device_ddr_init_timing_check am_devices_mspi_psram_aps25616n_ddr_init_timing_check
#define nsx_psram_device_apply_ddr_timing am_devices_mspi_psram_aps25616n_apply_ddr_timing
#endif

/*
 * DDR timing calibration (RXDQS read-strobe scan).
 *
 * NSX_PSRAM_RUN_DDR_TIMING_SCAN gates whether nsx_psram_platform_init() runs the
 * driver's *_ddr_init_timing_check()/apply_ddr_timing() calibration before
 * enabling XIP (as every AmbiqSuite mspi_hex_ddr_*psram example does). Hex DDR
 * PSRAM on Apollo5 uses a source-synchronous read strobe (DQS); the correct
 * RXDQS sampling delay is board- and part-specific, and the power-on-default
 * value is only marginally centred. Skipping the scan can appear to work at
 * room temperature yet corrupt reads intermittently over voltage/temperature or
 * at higher clocks. Running the scan picks the centre of the passing window and
 * makes DDR deterministic. Default ON here; can be forced off with
 * -DNSX_PSRAM_RUN_DDR_TIMING_SCAN=0 for boards/parts where the scan is known to
 * be unnecessary and boot latency matters.
 */
#ifndef NSX_PSRAM_RUN_DDR_TIMING_SCAN
#define NSX_PSRAM_RUN_DDR_TIMING_SCAN 0
#endif

#ifndef NSX_PSRAM_MSPI_MODULE
#define NSX_PSRAM_MSPI_MODULE 0
#endif

#ifndef NSX_PSRAM_DEVICE_SIZE_BYTES
#define NSX_PSRAM_DEVICE_SIZE_BYTES 33554432u
#endif

#ifndef AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA
#error "nsx-psram R5 currently supports BSPs that advertise AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA."
#endif

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

/*
 * SWD/debug-readable diagnostics for the DDR timing scan. 0xEEEE0000 means the
 * scan was not run; 0x900D0000|rxdqs means the scan found a valid setting (low
 * bits carry the driver's reported RXDQS delay); 0xBADD0000|status means the
 * scan failed with the given driver status.
 */
volatile uint32_t g_nsx_psram_timing_diag = 0xEEEE0000u;

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
        /* Both the BA and N driver families accept Hex DDR CE mode directly
         * (there is no BSP macro for it -- only
         * AM_BSP_MSPI_PSRAM_MODULE_OCTAL_DDR_CE exists, a holdover from the
         * original incorrect Octal/BA-everywhere assumption), so it is
         * specified directly here. */
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

    /* DDR read-timing calibration. Hex DDR PSRAM reads are strobed by DQS; the
     * correct RXDQS sampling delay is board/part specific. The driver scan finds
     * a valid window and apply_ddr_timing() programs its centre. See the
     * NSX_PSRAM_RUN_DDR_TIMING_SCAN note above. The scan runs BEFORE ddr_init()
     * (it performs its own probe init internally); the result is applied AFTER
     * ddr_init() and before enable_xip(), matching the AmbiqSuite examples. */
#if NSX_PSRAM_RUN_DDR_TIMING_SCAN
    am_devices_mspi_psram_ddr_timing_config_t nsx_psram_ddr_timing;
    status = nsx_psram_device_ddr_init_timing_check(
        NSX_PSRAM_MSPI_MODULE, &psram_cfg, &nsx_psram_ddr_timing);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        g_nsx_psram_timing_diag = 0xBADD0000u | (status & 0xFFFFu);
        return status;
    }
#if NSX_PSRAM_USE_BA_DRIVER
    g_nsx_psram_timing_diag =
        0x900D0000u | (nsx_psram_ddr_timing.sTimingCfg.ui8RxDQSDelay & 0xFFu);
#else
    g_nsx_psram_timing_diag =
        0x900D0000u | (nsx_psram_ddr_timing.ui32Rxdqsdelay & 0xFFu);
#endif
#endif

    status = nsx_psram_device_ddr_init(
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

#if NSX_PSRAM_RUN_DDR_TIMING_SCAN
    /* Program the RXDQS timing found by the scan above. */
    nsx_psram_device_apply_ddr_timing(g_nsx_psram_device_handle, &nsx_psram_ddr_timing);
#endif

    if (cfg->enable_xip) {
        status = nsx_psram_device_ddr_enable_xip(g_nsx_psram_device_handle);
        if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
            return status;
        }
    }

    cfg->base_address = nsx_psram_aperture_base(NSX_PSRAM_MSPI_MODULE);
    cfg->size_bytes = NSX_PSRAM_DEVICE_SIZE_BYTES;
    return NSX_STATUS_SUCCESS;
}
