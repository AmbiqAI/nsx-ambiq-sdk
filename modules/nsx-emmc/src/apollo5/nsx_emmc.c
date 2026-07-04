#include "nsx_emmc.h"

#include "am_bsp.h"
#include "am_util.h"
#include "nsx_interrupt.h"

static am_hal_card_host_t *g_nsx_emmc_host = NULL;
static am_hal_card_t g_nsx_emmc_card;

//
// Registered with nsx-interrupt for the configured SDIO/SDHC line, mirroring
// nsx-psram/nsx-nvm's IRQ-ownership shape: the module owns device-level
// servicing, nsx-interrupt owns the raw vector table and NVIC priority
// policy, and the app/board retains control over interrupt priority (rather
// than this module reaching directly into NVIC, which is the kind of
// app-policy overreach old neuralSPOT modules were prone to).
//
static void nsx_emmc_irq_handler(void *ctx) {
    uint32_t status;

    (void)ctx;

    if (g_nsx_emmc_host == NULL || g_nsx_emmc_host->pHandle == NULL) {
        return;
    }

    am_hal_sdhc_intr_status_get(g_nsx_emmc_host->pHandle, &status, true);
    am_hal_sdhc_intr_status_clear(g_nsx_emmc_host->pHandle, status);
    am_hal_sdhc_interrupt_service(g_nsx_emmc_host->pHandle, status);
}

uint32_t nsx_emmc_platform_init(nsx_emmc_config_t *cfg) {
    uint32_t status;

    /*
     * Power on the eMMC's VCC/VCCQ rails before touching SDIO. This is NOT
     * called by any AmbiqSuite reference example in this SDK's provider
     * (confirmed by searching every emmc_ and sdio_ prefixed example source
     * file in the AmbiqSuite tree), yet without it, am_hal_card_host_find_card()
     * and every subsequent SDIO command hangs indefinitely on real hardware,
     * with no error returned -- confirmed empirically on both apollo510_evb
     * and apollo510b_evb.
     */
    am_bsp_emmc_power_on(cfg->sdio_instance);
    am_util_delay_ms(50);

    am_bsp_sdio_pins_enable(cfg->sdio_instance, (uint8_t)cfg->bus_width);

    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = cfg->sdio_instance == 0 ? SDIO0_IRQn : SDIO1_IRQn,
        .handler = nsx_emmc_irq_handler,
        .ctx = NULL,
        .priority = AM_IRQ_PRIORITY_DEFAULT,
        .enable = true,
    };
    status = nsx_irq_register(&irq_cfg);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    am_hal_interrupt_master_enable();

    g_nsx_emmc_host = am_hal_get_card_host(
        cfg->sdio_instance == 0 ? AM_HAL_SDHC_CARD_HOST : AM_HAL_SDHC_CARD_HOST1, true);
    if (g_nsx_emmc_host == NULL) {
        return NSX_STATUS_FAILURE;
    }

    status = am_hal_card_host_find_card(g_nsx_emmc_host, &g_nsx_emmc_card);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    status = am_hal_card_init(&g_nsx_emmc_card, AM_HAL_CARD_TYPE_EMMC, NULL,
                              AM_HAL_CARD_PWR_CTRL_SDHC_OFF);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    status = am_hal_card_cfg_set(
        &g_nsx_emmc_card, AM_HAL_CARD_TYPE_EMMC,
        cfg->bus_width == NSX_EMMC_BUS_WIDTH_8   ? AM_HAL_HOST_BUS_WIDTH_8
        : cfg->bus_width == NSX_EMMC_BUS_WIDTH_4 ? AM_HAL_HOST_BUS_WIDTH_4
                                                  : AM_HAL_HOST_BUS_WIDTH_1,
        cfg->clock_freq_hz, AM_HAL_HOST_BUS_VOLTAGE_1_8, AM_HAL_HOST_UHS_NONE);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    /* Card capacity introspection (CSD/EXT-CSD parsing) is not implemented
     * yet; block_count is left at its default-config value of 0. */
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_emmc_platform_block_write(uint32_t start_block, uint32_t num_blocks,
                                        const uint8_t *buf) {
    return am_hal_card_block_write_sync(&g_nsx_emmc_card, start_block, num_blocks,
                                        (uint8_t *)buf);
}

uint32_t nsx_emmc_platform_block_read(uint32_t start_block, uint32_t num_blocks, uint8_t *buf) {
    return am_hal_card_block_read_sync(&g_nsx_emmc_card, start_block, num_blocks, buf);
}
