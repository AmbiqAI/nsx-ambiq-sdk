#ifndef TEST_AM_MCU_APOLLO_H
#define TEST_AM_MCU_APOLLO_H
#include <stdint.h>
#include <stdbool.h>
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_STATUS_FAIL 1u
#define AM_HAL_CLKGEN_FREQ_MAX_HZ 96000000u
extern uint32_t SystemCoreClock;
typedef struct {struct {unsigned MCUPERFACK,MCUPERFSTATUS;} MCUPERFREQ_b;} power_regs;
extern power_regs power;
#define PWRCTRL (&power)
#if defined(TEST_APOLLO3)
/* Use the shipped family declarations for initialization and burst outputs. */
#include "am_hal_burst.h"
#else
/* Distinct values catch accidental casts from the public NSX mode enum. */
typedef enum {
    AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER = 10,
    AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE = 20
} am_hal_pwrctrl_mcu_mode_e;
uint32_t am_hal_pwrctrl_mcu_mode_select(am_hal_pwrctrl_mcu_mode_e mode);
#endif
#endif
