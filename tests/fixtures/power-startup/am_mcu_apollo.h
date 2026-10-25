#ifndef NSX_TEST_POWER_HAL_H
#define NSX_TEST_POWER_HAL_H
#include <stdint.h>
#include <stdbool.h>
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_MCUCTRL_SRAM_PREFETCH_DATA 7u
#define AM_HAL_MCUCTRL_CONTROL_SRAM_PREFETCH 8u
#define AM_HAL_SYSCTRL_SLEEP_DEEP 9u
#define AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC 10u
#define AM_HAL_GPIO_OUTPUT_CLEAR 0u
#define AM_HAL_GPIO_OUTPUT_SET 1u
#define AM_DEVICES_BLECTRLR_RESET_PIN 12u
#define VCOMP_PWDKEY_PWDKEY_Key 0x37u
#define _VAL2FLD(field, value) (value)
typedef struct { uint32_t PWDKEY; } test_vcomp_t;
extern test_vcomp_t test_vcomp;
#define VCOMP (&test_vcomp)
typedef unsigned am_hal_cachectrl_config_t;
extern const am_hal_cachectrl_config_t am_hal_cachectrl_defaults;
extern const unsigned am_hal_gpio_pincfg_output;
uint32_t am_hal_cachectrl_config(const am_hal_cachectrl_config_t *);
uint32_t am_hal_cachectrl_enable(void);
uint32_t am_hal_mcuctrl_control(unsigned, void *);
uint32_t am_hal_clkgen_control(unsigned, void *);
uint32_t am_hal_gpio_pinconfig(uint32_t, unsigned);
uint32_t am_hal_gpio_state_write(uint32_t, unsigned);
void am_hal_sysctrl_sleep(unsigned);

typedef enum {
 AM_HAL_PWRCTRL_PERIPH_IOS, AM_HAL_PWRCTRL_PERIPH_IOM0, AM_HAL_PWRCTRL_PERIPH_IOM1,
 AM_HAL_PWRCTRL_PERIPH_IOM2, AM_HAL_PWRCTRL_PERIPH_IOM3, AM_HAL_PWRCTRL_PERIPH_IOM4,
 AM_HAL_PWRCTRL_PERIPH_IOM5, AM_HAL_PWRCTRL_PERIPH_IOM6, AM_HAL_PWRCTRL_PERIPH_IOM7,
 AM_HAL_PWRCTRL_PERIPH_UART0, AM_HAL_PWRCTRL_PERIPH_UART1, AM_HAL_PWRCTRL_PERIPH_UART2,
 AM_HAL_PWRCTRL_PERIPH_UART3, AM_HAL_PWRCTRL_PERIPH_ADC, AM_HAL_PWRCTRL_PERIPH_MSPI0,
 AM_HAL_PWRCTRL_PERIPH_MSPI1, AM_HAL_PWRCTRL_PERIPH_MSPI2, AM_HAL_PWRCTRL_PERIPH_GFX,
 AM_HAL_PWRCTRL_PERIPH_DISP, AM_HAL_PWRCTRL_PERIPH_DISPPHY, AM_HAL_PWRCTRL_PERIPH_USB,
 AM_HAL_PWRCTRL_PERIPH_USBPHY, AM_HAL_PWRCTRL_PERIPH_PDM0, AM_HAL_PWRCTRL_PERIPH_PDM1,
 AM_HAL_PWRCTRL_PERIPH_PDM2, AM_HAL_PWRCTRL_PERIPH_PDM3, AM_HAL_PWRCTRL_PERIPH_I2S0,
 AM_HAL_PWRCTRL_PERIPH_I2S1, AM_HAL_PWRCTRL_PERIPH_SDIO, AM_HAL_PWRCTRL_PERIPH_DEBUG,
 AM_HAL_PWRCTRL_PERIPH_AUDREC, AM_HAL_PWRCTRL_PERIPH_AUDPB, AM_HAL_PWRCTRL_PERIPH_AUDADC,
 TEST_PERIPHERAL_COUNT
} am_hal_pwrctrl_periph_e;
#define AM_HAL_PWRCTRL_CONTROL_CRYPTO_POWERDOWN 20u
#define AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP 21u
#define AM_HAL_PWRCTRL_SRAM_NONE 0u
#define AM_HAL_DSP0 0u
#define AM_HAL_DAXI_CONFIG_AGING_1024 1024u
#define AM_HAL_DAXI_CONFIG_NUMBUF_32 32u
#define AM_HAL_DAXI_CONFIG_NUMFREEBUF_3 3u
#define AM_HAL_PWRCTRL_CACHE_ALL 3u
#define AM_HAL_PWRCTRL_DTCM_128K 128u
typedef struct { unsigned eSRAMCfg, eActiveWithMCU, eActiveWithDSP, eSRAMRetain; } am_hal_pwrctrl_sram_memcfg_t;
typedef struct { bool bEnableICache, bRetainCache, bEnableRAM, bActiveRAM, bRetainRAM; } am_hal_pwrctrl_dsp_memory_config_t;
typedef struct { bool bDaxiPassThrough, bAgingSEnabled; unsigned eAgingCounter, eNumBuf, eNumFreeBuf; } am_hal_daxi_config_t;
typedef struct { unsigned eCacheCfg; bool bRetainCache; unsigned eDTCMCfg, eRetainDTCM; bool bEnableNVM0, bRetainNVM0; } am_hal_pwrctrl_mcu_memory_config_t;
uint32_t am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e);
uint32_t am_hal_pwrctrl_control(unsigned, void *);
uint32_t am_hal_pwrctrl_sram_config(am_hal_pwrctrl_sram_memcfg_t *);
uint32_t am_hal_pwrctrl_dsp_memory_config(unsigned, am_hal_pwrctrl_dsp_memory_config_t *);
uint32_t am_hal_daxi_config(const am_hal_daxi_config_t *);
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *);
#endif
