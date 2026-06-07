/**
 * @file nsx_timer_ap3.c
 * @brief Apollo3-specific timer implementation
 */

#include "nsx_timer.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_interrupt.h"

extern nsx_timer_config_t *nsx_timer_config[NSX_TIMER_TEMPCO + 1];

static bool g_nsx_stimer_initialized;
static uint32_t g_nsx_stimer_config_bits;

static uint32_t nsx_timer_stimer_compare_idx(nsx_timers_e timer_num) {
    switch (timer_num) {
    case NSX_TIMER_INTERRUPT:
        return 0;
    case NSX_TIMER_USB:
        return 2;
    case NSX_TIMER_TEMPCO:
        return 3;
    default:
        return 1;
    }
}

static uint32_t nsx_timer_stimer_compare_int(uint32_t compare_idx) {
    switch (compare_idx) {
    case 0:
        return AM_HAL_STIMER_INT_COMPAREA;
    case 1:
        return AM_HAL_STIMER_INT_COMPAREB;
    case 2:
        return AM_HAL_STIMER_INT_COMPAREC;
    case 3:
        return AM_HAL_STIMER_INT_COMPARED;
    case 4:
        return AM_HAL_STIMER_INT_COMPAREE;
    case 5:
        return AM_HAL_STIMER_INT_COMPAREF;
    case 6:
        return AM_HAL_STIMER_INT_COMPAREG;
    default:
        return AM_HAL_STIMER_INT_COMPAREH;
    }
}

static uint32_t nsx_timer_stimer_compare_enable(uint32_t compare_idx) {
    switch (compare_idx) {
    case 0:
        return AM_HAL_STIMER_CFG_COMPARE_A_ENABLE;
    case 1:
        return AM_HAL_STIMER_CFG_COMPARE_B_ENABLE;
    case 2:
        return AM_HAL_STIMER_CFG_COMPARE_C_ENABLE;
    case 3:
        return AM_HAL_STIMER_CFG_COMPARE_D_ENABLE;
    case 4:
        return AM_HAL_STIMER_CFG_COMPARE_E_ENABLE;
    case 5:
        return AM_HAL_STIMER_CFG_COMPARE_F_ENABLE;
    case 6:
        return AM_HAL_STIMER_CFG_COMPARE_G_ENABLE;
    default:
        return AM_HAL_STIMER_CFG_COMPARE_H_ENABLE;
    }
}

static IRQn_Type nsx_timer_stimer_irqn(uint32_t compare_idx) {
    return (IRQn_Type)(STIMER_CMPR0_IRQn + compare_idx);
}

static uint32_t nsx_timer_period_ticks(const nsx_timer_config_t *cfg) {
    return cfg->periodInMicroseconds * 3u;
}

static void nsx_timer_irq_handler(void *ctx) {
    nsx_timer_config_t *timer_cfg = (nsx_timer_config_t *)ctx;
    uint32_t compare_idx;
    uint32_t interrupt_mask;

    if (timer_cfg == NULL || timer_cfg->callback == NULL) {
        return;
    }

    compare_idx = nsx_timer_stimer_compare_idx(timer_cfg->timer);
    interrupt_mask = nsx_timer_stimer_compare_int(compare_idx);

    am_hal_stimer_int_clear(interrupt_mask);
    am_hal_stimer_compare_delta_set(compare_idx, nsx_timer_period_ticks(timer_cfg));
    timer_cfg->callback(timer_cfg);
}

static uint32_t nsx_timer_stimer_init(nsx_timer_config_t *cfg) {
    uint32_t compare_idx = nsx_timer_stimer_compare_idx(cfg->timer);
    uint32_t interrupt_mask = nsx_timer_stimer_compare_int(compare_idx);
    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = nsx_timer_stimer_irqn(compare_idx),
        .handler = nsx_timer_irq_handler,
        .ctx = cfg,
        .priority = 0,
        .enable = true,
    };
    uint32_t status;

    g_nsx_stimer_config_bits |= nsx_timer_stimer_compare_enable(compare_idx);
    status = am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ | AM_HAL_STIMER_CFG_RUN | g_nsx_stimer_config_bits);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    if (!g_nsx_stimer_initialized) {
        am_hal_stimer_counter_clear();
        g_nsx_stimer_initialized = true;
    }

    am_hal_stimer_int_clear(interrupt_mask);
    am_hal_stimer_compare_delta_set(compare_idx, nsx_timer_period_ticks(cfg));
    am_hal_stimer_int_enable(interrupt_mask);

    return nsx_irq_register(&irq_cfg);
}

static uint32_t nsx_timer_counter_init(void) {
    uint32_t status = am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ | AM_HAL_STIMER_CFG_RUN | g_nsx_stimer_config_bits);

    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    if (!g_nsx_stimer_initialized) {
        am_hal_stimer_counter_clear();
        g_nsx_stimer_initialized = true;
    }

    return AM_HAL_STATUS_SUCCESS;
}

uint32_t nsx_timer_platform_init(nsx_timer_config_t *cfg) {
    uint32_t ui32Status = AM_HAL_STATUS_SUCCESS;

#ifndef NSX_DISABLE_API_VALIDATION
    // Apollo3 uses STIMER for the shared microsecond ticker.
#endif

    if (cfg->enableInterrupt) {
        if (cfg->periodInMicroseconds == 0) {
            return NSX_STATUS_INVALID_CONFIG;
        }
        return nsx_timer_stimer_init(cfg);
    }

    ui32Status = nsx_timer_counter_init();
    return ui32Status;
}

uint32_t nsx_timer_us_read(nsx_timer_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return 0xDEADBEEF;
    }
#endif
    (void)cfg;
    return am_hal_stimer_counter_get() / 3;
}

uint32_t nsx_timer_clear(nsx_timer_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
#endif
    (void)cfg;
    am_hal_stimer_counter_clear();
    return NSX_STATUS_SUCCESS;
}
