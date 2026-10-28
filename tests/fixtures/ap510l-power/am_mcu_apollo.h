#ifndef TEST_AP510L_HAL_H
#define TEST_AP510L_HAL_H
#include <stdint.h>
#include <stdbool.h>
#define AM_HAL_STATUS_SUCCESS 0
#define AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC 1
#define AM_HAL_RTC_OSC_LFRC 2
#define AM_HAL_SYSCTRL_SLEEP_DEEP 3
#define AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL 4
#define AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP 5
#define VCOMP_PWDKEY_PWDKEY_Key 0x37
#define _VAL2FLD(field, value) (value)
#define AM_HAL_GPIO_MAX_PADS 8
#define SCB_CCR_IC_Msk 1
#define SCB_CCR_DC_Msk 2
#define __DSB() ((void)0)
#define __ISB() ((void)0)
typedef struct { unsigned PWDKEY; } vcomp_t;
typedef struct { unsigned DBGCTRL; struct { unsigned CRYPTOCLKGATEN, MRAM0PWRCTRL, MRAM0LPREN, MRAM0SLPEN; } MRAMCRYPTOPWRCTRL_b; } mcuctrl_t;
typedef struct { struct { unsigned PWRENNVM;
#if !defined(AM_PART_APOLLO510L) && !defined(AM_PART_APOLLO330P)
unsigned PWRENNVM1;
#endif
 } MEMPWREN_b; } pwrctrl_t;
typedef struct { unsigned ICIALLU, CCR; } scb_t;
extern vcomp_t vcomp; extern mcuctrl_t mcuctrl; extern pwrctrl_t pwrctrl; extern scb_t scb;
#define VCOMP (&vcomp)
#define MCUCTRL (&mcuctrl)
#define PWRCTRL (&pwrctrl)
#define SCB (&scb)
/* Deliberately omit peripherals, memory enums and registers absent on AP510L. */
typedef enum { AM_HAL_PWRCTRL_PERIPH_IOM0, AM_HAL_PWRCTRL_PERIPH_IOM1,
 AM_HAL_PWRCTRL_PERIPH_IOM2, AM_HAL_PWRCTRL_PERIPH_IOM3, AM_HAL_PWRCTRL_PERIPH_IOM4,
 AM_HAL_PWRCTRL_PERIPH_IOM5, AM_HAL_PWRCTRL_PERIPH_UART0, AM_HAL_PWRCTRL_PERIPH_UART1,
 AM_HAL_PWRCTRL_PERIPH_USB, AM_HAL_PWRCTRL_PERIPH_USBPHY, AM_HAL_PWRCTRL_PERIPH_DEBUG,
 AM_HAL_PWRCTRL_PERIPH_CRYPTO, AM_HAL_PWRCTRL_PERIPH_OTP, TEST_PERIPH_COUNT
} am_hal_pwrctrl_periph_e;
enum { AM_HAL_PWRCTRL_ROM_AUTO=1, AM_HAL_PWRCTRL_DTCM128K=2, AM_HAL_PWRCTRL_DTCM256K=3,
 AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN=4, AM_HAL_PWRCTRL_NVM=5,
 AM_HAL_PWRCTRL_SRAM_NONE=0, AM_HAL_PWRCTRL_SRAM_1P75M=7,
 AM_HAL_PWRCTRL_RLP_ON=8, AM_HAL_PWRCTRL_ELP_ON=9, AM_HAL_PWRCTRL_CLP_ON=10 };
typedef struct { unsigned eROMMode,eDTCMCfg,eRetainDTCM,eNVMCfg; bool bKeepNVMOnInDeepSleep; } am_hal_pwrctrl_mcu_memory_config_t;
typedef struct { unsigned eSRAMCfg,eActiveWithMCU,eActiveWithGFX,eActiveWithDISP,eSRAMRetain; } am_hal_pwrctrl_sram_memcfg_t;
typedef struct { unsigned eRlpConfig,eElpConfig,eClpConfig; } am_hal_pwrctrl_pwrmodctl_cpdlp_t;
uint32_t am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e);
uint32_t am_hal_clkgen_control(unsigned, void *);
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *);
uint32_t am_hal_pwrctrl_sram_config(am_hal_pwrctrl_sram_memcfg_t *);
uint32_t am_hal_pwrctrl_pwrmodctl_cpdlp_config(am_hal_pwrctrl_pwrmodctl_cpdlp_t);
uint32_t am_hal_cachectrl_icache_enable(void);
uint32_t am_hal_cachectrl_dcache_enable(bool);
void am_hal_rtc_osc_select(uint32_t);
uint32_t am_hal_rtc_osc_disable(void);
void am_hal_sysctrl_fpu_enable(void);
void am_hal_sysctrl_fpu_stacking_enable(bool);
uint32_t am_hal_pwrctrl_rss_pwroff(void);
void am_hal_sysctrl_sleep(unsigned);
uint32_t am_hal_pwrctrl_control(unsigned, void *);
void am_hal_timer_stop(uint32_t);
uint32_t am_hal_gpio_pinconfig(uint32_t, unsigned);
extern const unsigned am_hal_gpio_pincfg_disabled;
uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t);
#endif
