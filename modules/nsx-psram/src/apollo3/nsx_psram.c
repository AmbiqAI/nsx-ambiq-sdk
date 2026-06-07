#include "nsx_psram.h"

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

uint32_t nsx_psram_platform_init(nsx_psram_config_t *cfg) {
    uint32_t status;
    am_devices_mspi_psram_sdr_timing_config_t timing;
    am_devices_mspi_psram_config_t psram_cfg = {
        .eDeviceConfig = AM_HAL_MSPI_FLASH_QUAD_CE0,
        .eClockFreq = cfg->clock_freq,
        .pNBTxnBuf = cfg->nbtxn_buf != NULL ? cfg->nbtxn_buf : g_nsx_psram_dma_buffer,
        .ui32NBTxnBufLength = cfg->nbtxn_buf_len != 0
            ? cfg->nbtxn_buf_len
            : (uint32_t)(sizeof(g_nsx_psram_dma_buffer) / sizeof(g_nsx_psram_dma_buffer[0])),
        .ui32ScramblingStartAddr = cfg->scrambling_start_addr,
        .ui32ScramblingEndAddr = cfg->scrambling_end_addr,
    };

    status = am_devices_mspi_psram_sdr_init_timing_check(
        AM_BSP_MSPI_PSRAM_INST, &psram_cfg, &timing);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        return status;
    }

    status = am_devices_mspi_psram_init(
        AM_BSP_MSPI_PSRAM_INST, &psram_cfg, &g_nsx_psram_device_handle, &g_nsx_psram_mspi_handle);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        return status;
    }

    status = am_devices_mspi_psram_apply_sdr_timing(g_nsx_psram_device_handle, &timing);
    if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
        return status;
    }

    if (cfg->enable_xip) {
        status = am_devices_mspi_psram_enable_xip(g_nsx_psram_device_handle);
        if (status != AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS) {
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
        return status;
    }
    am_hal_interrupt_master_enable();

    cfg->base_address = cfg->enable_xip ? NSX_PSRAM_XIP_BASE : 0u;
    cfg->size_bytes = NSX_PSRAM_DEVICE_SIZE_BYTES;
    return NSX_STATUS_SUCCESS;
}
