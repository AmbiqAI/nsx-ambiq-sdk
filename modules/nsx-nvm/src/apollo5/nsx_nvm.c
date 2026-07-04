#include "nsx_nvm.h"

#include "am_bsp.h"
#include "am_devices_mspi_is25wx064.h"
#include "nsx_interrupt.h"

#ifndef NSX_NVM_MSPI_MODULE
#define NSX_NVM_MSPI_MODULE 1
#endif

#ifndef NSX_NVM_DEVICE_SIZE_BYTES
#define NSX_NVM_DEVICE_SIZE_BYTES (8u * 1024u * 1024u) /* IS25WX064 is 8MB */
#endif

#ifndef AM_BSP_MSPI_FLASH_DEVICE_IS25WX064
#error "nsx-nvm currently supports BSPs that advertise AM_BSP_MSPI_FLASH_DEVICE_IS25WX064."
#endif

/*
 * The MSPI instance is fixed at compile time (NSX_NVM_MSPI_MODULE), so the
 * IRQ and aperture are selected by macro. This avoids referencing MSPI
 * symbols (e.g. MSPI3_IRQn) that do not exist on every supported SoC family.
 */
#define NSX_NVM_CONCAT_(a, b, c) a##b##c
#define NSX_NVM_MSPI_SYM(prefix, n, suffix) NSX_NVM_CONCAT_(prefix, n, suffix)
#define NSX_NVM_MSPI_IRQ NSX_NVM_MSPI_SYM(MSPI, NSX_NVM_MSPI_MODULE, _IRQn)
#define NSX_NVM_MSPI_APERTURE NSX_NVM_MSPI_SYM(MSPI, NSX_NVM_MSPI_MODULE, _APERTURE_START_ADDR)

/* Non-blocking transaction (DMA/TCB) buffer used when the caller does not
 * supply their own via nsx_nvm_config_t. */
AM_SHARED_RW static uint32_t g_nsx_nvm_dma_buffer[256];

static void *g_nsx_nvm_device_handle = NULL;
static void *g_nsx_nvm_mspi_handle = NULL;

static void nsx_nvm_configure_mpu(void) {
    am_hal_mpu_region_config_t mpu_cfg = {
        .ui32RegionNumber = 6,
        .ui32BaseAddress = (uint32_t)g_nsx_nvm_dma_buffer,
        .eShareable = NON_SHARE,
        .eAccessPermission = RW_NONPRIV,
        .bExecuteNever = true,
        .ui32LimitAddress = (uint32_t)g_nsx_nvm_dma_buffer + sizeof(g_nsx_nvm_dma_buffer) - 1,
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

static am_hal_mspi_device_e nsx_nvm_device_config(const nsx_nvm_config_t *cfg) {
    if (cfg->iface == NSX_NVM_IF_OCTAL) {
        return cfg->chip_select == 0 ? AM_HAL_MSPI_FLASH_OCTAL_CE0
                                      : AM_HAL_MSPI_FLASH_OCTAL_CE1;
    }
    if (cfg->iface == NSX_NVM_IF_OCTAL_1_8_8) {
        return cfg->chip_select == 0 ? AM_HAL_MSPI_FLASH_OCTAL_CE0_1_8_8
                                      : AM_HAL_MSPI_FLASH_OCTAL_CE1_1_8_8;
    }
    return cfg->chip_select == 0 ? AM_HAL_MSPI_FLASH_OCTAL_DDR_CE0
                                  : AM_HAL_MSPI_FLASH_OCTAL_DDR_CE1;
}

//
// Registered with nsx-interrupt for the configured MSPI line. The raw vector
// glue (am_mspiN_isr) lives in nsx-interrupt; this module only owns the
// device-level servicing. This mirrors nsx-psram's IRQ ownership model:
// nsx-nvm never touches NVIC directly.
//
static void nsx_nvm_irq_handler(void *ctx) {
    uint32_t status;

    (void)ctx;

    if (g_nsx_nvm_mspi_handle == NULL) {
        return;
    }

    am_hal_mspi_interrupt_status_get(g_nsx_nvm_mspi_handle, &status, false);
    am_hal_mspi_interrupt_clear(g_nsx_nvm_mspi_handle, status);
    am_hal_mspi_interrupt_service(g_nsx_nvm_mspi_handle, status);
}

uint32_t nsx_nvm_platform_init(nsx_nvm_config_t *cfg) {
    uint32_t status;
    am_devices_mspi_is25wx064_config_t nvm_cfg = {
        .eDeviceConfig = nsx_nvm_device_config(cfg),
        .eClockFreq = cfg->clock_freq,
        .pNBTxnBuf = cfg->nbtxn_buf != NULL ? cfg->nbtxn_buf : g_nsx_nvm_dma_buffer,
        .ui32NBTxnBufLength = cfg->nbtxn_buf_len != 0
            ? cfg->nbtxn_buf_len
            : (uint32_t)(sizeof(g_nsx_nvm_dma_buffer) / sizeof(g_nsx_nvm_dma_buffer[0])),
        .ui32ScramblingStartAddr = cfg->scrambling_start_addr,
        .ui32ScramblingEndAddr = cfg->scrambling_end_addr,
    };

    if (cfg->configure_mpu) {
        nsx_nvm_configure_mpu();
    }

    /* DDR timing calibration is intentionally not called here. AmbiqSuite's
     * own reference example only invokes it when MSPI_TIMING_SCAN is
     * explicitly defined (opt-in, not the default), and
     * am_devices_mspi_is25wx064_init_timing_check() runs its own internal
     * init()+erase() cycle for calibration; if that internal erase fails, it
     * returns without deiniting, permanently leaking the driver's static
     * device-handle slot and causing every subsequent nsx_nvm_init() call to
     * fail with "no free device slot" -- a misleading symptom that looks
     * unrelated to timing at all. apply_ddr_timing() is safe to skip too:
     * the HAL falls back to its own best-known defaults. */

    status = am_devices_mspi_is25wx064_init(
        NSX_NVM_MSPI_MODULE, &nvm_cfg, &g_nsx_nvm_device_handle, &g_nsx_nvm_mspi_handle);
    if (status != AM_DEVICES_MSPI_IS25WX064_STATUS_SUCCESS) {
        return status;
    }

    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = NSX_NVM_MSPI_IRQ,
        .handler = nsx_nvm_irq_handler,
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
        status = am_devices_mspi_is25wx064_enable_xip(g_nsx_nvm_device_handle);
        if (status != AM_DEVICES_MSPI_IS25WX064_STATUS_SUCCESS) {
            return status;
        }
        am_hal_cachectrl_dcache_invalidate(NULL, true);
    }

    cfg->xip_base_address = NSX_NVM_MSPI_APERTURE;
    cfg->size_bytes = NSX_NVM_DEVICE_SIZE_BYTES;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_nvm_platform_read(uint32_t addr, uint8_t *buf, uint32_t len, bool wait) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return am_devices_mspi_is25wx064_read(g_nsx_nvm_device_handle, buf, addr, len, wait);
}

uint32_t nsx_nvm_platform_write(uint32_t addr, const uint8_t *buf, uint32_t len, bool wait) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return am_devices_mspi_is25wx064_write(
        g_nsx_nvm_device_handle, (uint8_t *)buf, addr, len, wait);
}

uint32_t nsx_nvm_platform_sector_erase(uint32_t sector_addr) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return am_devices_mspi_is25wx064_sector_erase(g_nsx_nvm_device_handle, sector_addr);
}

uint32_t nsx_nvm_platform_mass_erase(void) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return am_devices_mspi_is25wx064_mass_erase(g_nsx_nvm_device_handle);
}

uint32_t nsx_nvm_platform_enable_xip(void) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    uint32_t status = am_devices_mspi_is25wx064_enable_xip(g_nsx_nvm_device_handle);
    if (status == AM_DEVICES_MSPI_IS25WX064_STATUS_SUCCESS) {
        am_hal_cachectrl_dcache_invalidate(NULL, true);
    }
    return status;
}

uint32_t nsx_nvm_platform_disable_xip(void) {
    if (g_nsx_nvm_device_handle == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    return am_devices_mspi_is25wx064_disable_xip(g_nsx_nvm_device_handle);
}
