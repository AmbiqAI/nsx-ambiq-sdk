#pragma once
#include "am_mcu_apollo.h"
#define AM_BSP_GPIO_ITM_SWO 28
#define g_AM_BSP_GPIO_ITM_SWO 1
void am_bsp_low_power_init(void);
#if defined(AM_PART_APOLLO3P)
void am_bsp_itm_printf_enable(void); void am_bsp_uart_printf_enable(void);
#else
uint32_t am_bsp_itm_printf_enable(void); uint32_t am_bsp_uart_printf_enable(void);
#endif
void am_bsp_itm_printf_disable(void); void am_bsp_uart_printf_disable(void);
void am_bsp_debug_printf_enable(void); void am_bsp_debug_printf_disable(void);
