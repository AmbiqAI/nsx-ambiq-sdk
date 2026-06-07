/**
 * @file nsx_timer_ap4.c
 * @author Ambiq
 * @brief Apollo5-family timer implementation
 * @version 0.1
 * @date 2022-10-11
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "nsx_timer.h"
#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_interrupt.h"
#include "nsx_core.h"

extern nsx_timer_config_t *nsx_timer_config[NSX_TIMER_TEMPCO + 1];

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

    if (timer_cfg == NULL || timer_cfg->callback == NULL) {
        return;
    }

    timer_num = timer_cfg->timer;
    am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(timer_num, AM_HAL_TIMER_COMPARE1));
    am_hal_timer_clear(timer_num);
    timer_cfg->callback(timer_cfg);
}

uint32_t nsx_timer_platform_init(nsx_timer_config_t *cfg) {
    uint32_t ui32Status = AM_HAL_STATUS_SUCCESS;

#ifndef NSX_DISABLE_API_VALIDATION
    // Apollo5-family API validation
#endif

    am_hal_timer_config_t TimerConfig;

    // Set the timer configuration
    // The default timer configuration is HFRC_DIV16, EDGE, compares=0, no trig.
    am_hal_timer_default_config_set(&TimerConfig);

    // modify the default
    if (cfg->timer == NSX_TIMER_TEMPCO) {
        TimerConfig.eInputClock = AM_HAL_TIMER_CLOCK_HFRC_DIV16;
    }

    if ((cfg->enableInterrupt)) {
        TimerConfig.eFunction = AM_HAL_TIMER_FN_UPCOUNT;
        TimerConfig.ui32Compare1 = cfg->periodInMicroseconds / 6; // 6 ticks per uS
    }

    ui32Status = am_hal_timer_config(cfg->timer, &TimerConfig);
    if (ui32Status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("Failed to configure TIMER%d, return value=%d\r\n", cfg->timer, ui32Status);
        return ui32Status;
    }

    //
    // Stop and clear the timer.
    //
    am_hal_timer_clear(cfg->timer);

    if ((cfg->enableInterrupt)) {
        am_hal_timer_interrupt_clear(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1));
        am_hal_timer_interrupt_enable(AM_HAL_TIMER_MASK(cfg->timer, AM_HAL_TIMER_COMPARE1));
        nsx_irq_config_t irq_cfg = {
            .api = &nsx_interrupt_current_version,
            .irqn = nsx_timer_irqn(cfg->timer),
            .handler = nsx_timer_irq_handler,
            .ctx = cfg,
            .priority = AM_IRQ_PRIORITY_DEFAULT,
            .enable = true,
        };
        NSX_TRY(nsx_irq_register(&irq_cfg), "Failed to register timer interrupt");
    }
    return ui32Status;
}

uint32_t nsx_timer_us_read(nsx_timer_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return 0xDEADBEEF;
    }
#endif
    return am_hal_timer_read(cfg->timer) / 6; // 6 ticks per uS
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
