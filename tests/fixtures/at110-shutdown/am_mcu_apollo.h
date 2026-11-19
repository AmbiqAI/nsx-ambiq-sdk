#pragma once
#include <stdint.h>

typedef struct { uint32_t PWDKEY; } mock_vcomp_t;
extern mock_vcomp_t vcomp;
#define VCOMP (&vcomp)
#define VCOMP_PWDKEY_PWDKEY_Key 0x37U

enum {
    AM_REG_NUM_TIMERS = 16,
    AM_HAL_INTERNAL_TIMER_NUM_A = 15,
    AM_HAL_INTERNAL_TIMER_NUM_B = 14,
    AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL = 11,
    AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP = 12,
};
uint32_t am_hal_pwrctrl_control(unsigned control, void *argument);
uint32_t am_hal_timer_disable(uint32_t timer);
#define am_hal_timer_stop(timer) am_hal_timer_disable(timer)
