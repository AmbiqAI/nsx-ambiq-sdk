#ifndef TIMER_TEST_HAL_H
#define TIMER_TEST_HAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int IRQn_Type;
enum { TIMER1_IRQn=1, TIMER2_IRQn, TIMER3_IRQn, TIMER4_IRQn };
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_TIMER_CLOCK_HFRC_DIV16 1
#define AM_HAL_TIMER_FN_UPCOUNT 2
#define AM_HAL_TIMER_COMPARE1 2u
#define AM_HAL_TIMER_MASK(n,c) ((c) << (2u * (n)))
#define AM_IRQ_PRIORITY_DEFAULT 3
typedef struct { int eInputClock,eFunction; uint32_t ui32Compare0,ui32Compare1; } am_hal_timer_config_t;
uint32_t am_hal_timer_default_config_set(am_hal_timer_config_t*);
uint32_t am_hal_timer_config(uint32_t,am_hal_timer_config_t*);
uint32_t am_hal_timer_clear(uint32_t);
uint32_t am_hal_timer_start(uint32_t);
uint32_t am_hal_timer_stop(uint32_t);
uint32_t am_hal_timer_interrupt_clear(uint32_t);
uint32_t am_hal_timer_interrupt_enable(uint32_t);
uint32_t am_hal_timer_interrupt_disable(uint32_t);
uint32_t am_hal_timer_read(uint32_t);
uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t);
#endif
