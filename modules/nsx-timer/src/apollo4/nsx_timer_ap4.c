/**
 * @file nsx_timer_ap4.c
 * @brief Apollo4-specific timer implementation
 */

#include "nsx_timer.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_interrupt.h"

extern nsx_timer_config_t *nsx_timer_config[NSX_TIMER_TEMPCO + 1];

typedef struct {
    volatile bool ready;
    bool interrupt;
    nsx_timer_callback_cb callback;
} nsx_timer_state_t;

static nsx_timer_state_t s_timer_state[NSX_TIMER_TEMPCO + 1];
uint32_t nsx_timer_platform_deinit(nsx_timer_config_t *cfg);

static IRQn_Type nsx_timer_irqn(nsx_timers_e timer_num) {
    switch (timer_num) {
    case NSX_TIMER_INTERRUPT:
        return TIMER1_IRQn;
    case NSX_TIMER_USB:
        return TIMER3_IRQn;
    case NSX_TIMER_TEMPCO:
        return TIMER4_IRQn;
    default:
        return TIMER2_IRQn;
    }
}

static void nsx_timer_irq_handler(void *ctx) {
    nsx_timer_config_t *timer_cfg = (nsx_timer_config_t *)ctx;
    nsx_timers_e timer_num;

    if (timer_cfg == NULL) {
        return;
    }

    timer_num = timer_cfg->timer;
    if ((unsigned)timer_num > NSX_TIMER_TEMPCO || nsx_timer_config[timer_num] != timer_cfg) {
        return;
    }
    am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(timer_num, AM_HAL_TIMER_COMPARE1));
    if (!s_timer_state[timer_num].ready || s_timer_state[timer_num].callback == NULL) {
        return;
    }
    am_hal_timer_clear(timer_num);
    s_timer_state[timer_num].callback(timer_cfg);
}

#define TIMER_TRY(operation) do { \
    uint32_t status = (operation); \
    if (status != NSX_STATUS_SUCCESS) return status; \
} while (0)

uint32_t nsx_timer_platform_init(nsx_timer_config_t *cfg) {
    if (cfg == NULL || (unsigned)cfg->timer > NSX_TIMER_TEMPCO) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (nsx_timer_config[cfg->timer] != NULL) {
        if (nsx_timer_config[cfg->timer] != cfg) return NSX_TIMER_STATUS_IN_USE;
        TIMER_TRY(nsx_timer_platform_deinit(cfg));
    }
    nsx_timer_config[cfg->timer] = cfg;
    s_timer_state[cfg->timer].ready = false;
    s_timer_state[cfg->timer].interrupt = cfg->enableInterrupt;
    s_timer_state[cfg->timer].callback = cfg->enableInterrupt ? cfg->callback : NULL;
    am_hal_timer_config_t timer_config;
    TIMER_TRY(am_hal_timer_default_config_set(&timer_config));
    if (cfg->timer == NSX_TIMER_TEMPCO) {
        timer_config.eInputClock = AM_HAL_TIMER_CLOCK_HFRC_DIV16;
    }
    if (cfg->enableInterrupt) {
        timer_config.eFunction = AM_HAL_TIMER_FN_UPCOUNT;
        timer_config.ui32Compare1 = cfg->periodInMicroseconds / 6;
    }
    TIMER_TRY(am_hal_timer_config(cfg->timer, &timer_config));
    TIMER_TRY(am_hal_timer_clear(cfg->timer));
    if (cfg->enableInterrupt) {
        TIMER_TRY(am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1)));
        TIMER_TRY(am_hal_timer_interrupt_enable(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1)));
        nsx_irq_config_t irq_cfg = {
            .api = &nsx_interrupt_current_version,
            .irqn = nsx_timer_irqn(cfg->timer),
            .handler = nsx_timer_irq_handler,
            .ctx = cfg,
            .priority = AM_IRQ_PRIORITY_DEFAULT,
            .enable = true,
        };
        TIMER_TRY(nsx_irq_register(&irq_cfg));
    }
    TIMER_TRY(am_hal_timer_start(cfg->timer));
    s_timer_state[cfg->timer].ready = true;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_timer_platform_deinit(nsx_timer_config_t *cfg) {
    if (cfg == NULL || (unsigned)cfg->timer > NSX_TIMER_TEMPCO) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (nsx_timer_config[cfg->timer] == NULL) return NSX_STATUS_SUCCESS;
    if (nsx_timer_config[cfg->timer] != cfg) return NSX_STATUS_INVALID_CONFIG;
    s_timer_state[cfg->timer].ready = false;
    if (s_timer_state[cfg->timer].interrupt) {
        TIMER_TRY(nsx_irq_unregister(nsx_timer_irqn(cfg->timer)));
        TIMER_TRY(am_hal_timer_interrupt_disable(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1)));
    }
    TIMER_TRY(am_hal_timer_stop(cfg->timer));
    TIMER_TRY(am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1)));
    if (s_timer_state[cfg->timer].interrupt) TIMER_TRY(nsx_irq_clear_pending(nsx_timer_irqn(cfg->timer)));
    nsx_timer_config[cfg->timer] = NULL;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_timer_us_read(nsx_timer_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return 0xDEADBEEF;
    }
#endif
    return am_hal_timer_read(cfg->timer) / 6;
}

uint32_t nsx_timer_clear(nsx_timer_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
#endif
    am_hal_timer_clear(cfg->timer);
    return NSX_STATUS_SUCCESS;
}
