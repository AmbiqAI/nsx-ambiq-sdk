#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "am_hal_burst.h"
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_STATUS_FAIL 1u
#define AM_HAL_STATUS_INVALID_OPERATION 2u
extern uint32_t SystemCoreClock;
typedef struct { struct {unsigned CORESEL;} CCTRL_b; struct {unsigned BURSTREQ, BURSTSTATUS;} FREQCTRL_b; } clock_regs;
typedef struct { struct {unsigned SIMOBUCKEN;} MISC_b; struct {unsigned SIMOBUCKON;} SUPPLYSTATUS_b; struct {unsigned BLEBUCKEN;} SUPPLYSRC_b; } power_regs;
typedef struct { struct {unsigned ALLOWBURST;} SKU_b; } mcu_regs;
extern clock_regs clock_hw;
extern power_regs power_hw;
extern mcu_regs mcu_hw;
#define CLKGEN (&clock_hw)
#define PWRCTRL (&power_hw)
#define MCUCTRL (&mcu_hw)
