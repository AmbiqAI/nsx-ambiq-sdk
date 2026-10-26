#pragma once
#include <stdint.h>
#include <stdbool.h>
#define AM_HAL_STATUS_SUCCESS 0
#define AM_HAL_STATUS_FAIL 1
#define AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER 10
#define AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE 11
#define AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1 12
#define AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2 13
#define AM_HAL_PWRCTRL_PERIPH_OTP 0
#define AM_HAL_PWRCTRL_PERIPH_CRYPTO 1
#define AM_HAL_PWRCTRL_RLP_ON 1
#define AM_HAL_PWRCTRL_ELP_ON 1
#define AM_HAL_PWRCTRL_CLP_ON 1
#define AM_HAL_DCU_CPUTRC_DWT_SWO 1
#define AM_HAL_DCU_CPUDBG_NON_INVASIVE 2
#define AM_HAL_DCU_CPUDBG_S_NON_INVASIVE 4
#define AM_HAL_DCU_CPUTRC_PERFCNT 8
#define AM_HAL_DCU_SWD 16
#define AM_HAL_DCU_TRACE 32
#define MCUCTRL_DBGCTRL_DBGTPIUCLKSEL_HFRC_96MHz 0
#define TPI_CSPSR_CWIDTH_1BIT 1
#define TPI_SPPR_TXMODE_UART 1
#define _VAL2FLD(f,v) (v)
#define AM_CRITICAL_BEGIN { uint32_t saved_mask = am_hal_interrupt_master_disable()
#define AM_CRITICAL_END am_hal_interrupt_master_set(saved_mask); }
typedef struct {struct {unsigned PWRSTOTP,PWRSTCRYPTO;} DEVPWRSTATUS_b;} power_t;
typedef struct {struct {unsigned HOSTCCISIDLE;} HOSTCCISIDLE_b;} crypto_t;
typedef struct {unsigned TPR,TER,TCR;} itm_t;
extern power_t power; extern crypto_t crypto; extern itm_t itm;
#define PWRCTRL (&power)
#define CRYPTO (&crypto)
#define ITM (&itm)
typedef unsigned am_hal_cachectrl_config_t;
extern const am_hal_cachectrl_config_t am_hal_cachectrl_defaults;
typedef struct {unsigned eRlpConfig,eElpConfig,eClpConfig;} am_hal_pwrctrl_pwrmodctl_cpdlp_t;
typedef union {unsigned PROFILE; struct {unsigned COLLAPSESTMANDSTMP:1;} PROFILE_b;} am_hal_spotmgr_profile_t;
uint32_t am_hal_interrupt_master_disable(void); void am_hal_interrupt_master_enable(void); void am_hal_interrupt_master_set(uint32_t);
uint32_t am_hal_cachectrl_config(const am_hal_cachectrl_config_t*); uint32_t am_hal_cachectrl_enable(void);
void am_hal_sysctrl_fpu_enable(void); void am_hal_sysctrl_fpu_stacking_enable(bool);
uint32_t am_hal_pwrctrl_mcu_mode_select(unsigned); uint32_t am_hal_pwrctrl_periph_enable(unsigned); uint32_t am_hal_pwrctrl_periph_disable(unsigned);
uint32_t am_hal_dcu_get(uint32_t*); uint32_t am_hal_dcu_update(bool,uint32_t);
uint32_t am_hal_pwrctrl_pwrmodctl_cpdlp_config(am_hal_pwrctrl_pwrmodctl_cpdlp_t);
uint32_t am_hal_spotmgr_init(void); void am_hal_spotmgr_profile_set(am_hal_spotmgr_profile_t*);
uint32_t am_hal_debug_enable(void); uint32_t am_hal_tpiu_config(unsigned,unsigned,unsigned,unsigned,unsigned);
uint32_t am_hal_gpio_pinconfig(unsigned,unsigned); void am_hal_itm_print(char*);
extern uint32_t SystemCoreClock;

#define AM_HAL_GPIO_OUTPUT_SET 1
#define am_hal_gpio_pincfg_output 2
#define am_hal_gpio_pincfg_default 3
uint32_t am_hal_itm_disable(void);
uint32_t am_hal_tpiu_disable(void);
uint32_t am_hal_gpio_state_write(unsigned, unsigned);
