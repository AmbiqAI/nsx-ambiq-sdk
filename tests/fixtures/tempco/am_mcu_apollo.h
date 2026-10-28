#ifndef TEST_TEMPCO_HAL_H
#define TEST_TEMPCO_HAL_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define AM_HAL_STATUS_SUCCESS 0
#define AM_HAL_TEMPCO_NUMSAMPLES 5
#define AM_HAL_STATUS_TIMEOUT 4u
#define AM_HAL_STATUS_HW_ERR 5u
#define AM_REG_INFO1_TRIM_REV_O 24
#define AM_HAL_ADC_CLKSEL_HFRC_24MHZ 1
#define AM_HAL_ADC_TRIGPOL_RISING 2
#define AM_HAL_ADC_TRIGSEL_SOFTWARE 3
#define AM_HAL_ADC_CLKMODE_LOW_POWER 4
#define AM_HAL_ADC_LPMODE1 5
#define AM_HAL_ADC_SINGLE_SCAN 6
#define AM_HAL_ADC_RPTTRIGSEL_TMR 7
#define AM_HAL_SYSCTRL_WAKE 1
#define AM_HAL_SYSCTRL_DEEPSLEEP 2
#define AM_HAL_PWRCTRL_PERIPH_ADC 3
#define AM_HAL_DAXI_CONTROL_INVALIDATE 4
#define AM_HAL_TIMER_CLOCK_HFRC_DIV16 1
#define AM_HAL_TIMER_FN_UPCOUNT 2
#define AM_HAL_TIMER_COMPARE1 1
#define AM_HAL_TIMER_MASK(n,c) (1u << (n))
#define AM_IRQ_PRIORITY_DEFAULT 3
#define _FLD2VAL(field,x) (x)
typedef int IRQn_Type;
enum { TIMER1_IRQn=1, TIMER2_IRQn, TIMER3_IRQn, TIMER4_IRQn };
typedef struct { uint32_t FIFO; } test_adc_t;
extern test_adc_t test_adc;
#define ADC (&test_adc)
typedef struct { int eClock,ePolarity,eTrigger,eClockMode,ePowerMode,eRepeat,eRepeatTrigger; } am_hal_adc_config_t;
typedef struct { uint32_t ui32Sample,ui32Slot; } am_hal_adc_sample_t;
typedef struct { int eInputClock,eFunction; uint32_t ui32Compare1; } am_hal_timer_config_t;
uint32_t am_hal_mram_info_read(uint32_t,uint32_t,uint32_t,uint32_t*);
uint32_t am_hal_adc_initialize(uint32_t,void**);
uint32_t am_hal_adc_deinitialize(void*);
uint32_t am_hal_adc_power_control(void*,int,bool);
uint32_t am_hal_adc_configure(void*,am_hal_adc_config_t*);
uint32_t am_hal_adc_enable(void*);
uint32_t am_hal_adc_disable(void*);
uint32_t am_hal_pwrctrl_periph_enable(int);
uint32_t am_hal_pwrctrl_periph_disable(int);
uint32_t am_hal_pwrctrl_tempco_init(void*,uint32_t);
uint32_t am_hal_pwrctrl_tempco_sample_handler(uint32_t,am_hal_adc_sample_t*);
uint32_t am_hal_adc_sw_trigger(void*);
uint32_t am_hal_daxi_control(int,void*);
uint32_t am_hal_adc_samples_read(void*,bool,void*,uint32_t*,am_hal_adc_sample_t*);
void am_hal_delay_us(uint32_t);
uint32_t am_hal_timer_default_config_set(am_hal_timer_config_t*);
uint32_t am_hal_timer_config(uint32_t,am_hal_timer_config_t*);
uint32_t am_hal_timer_clear(uint32_t);
uint32_t am_hal_timer_start(uint32_t);
uint32_t am_hal_timer_stop(uint32_t);
uint32_t am_hal_timer_interrupt_clear(uint32_t);
uint32_t am_hal_timer_interrupt_enable(uint32_t);
uint32_t am_hal_timer_interrupt_disable(uint32_t);
uint32_t am_hal_timer_read(uint32_t);
#endif

uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t mask);
