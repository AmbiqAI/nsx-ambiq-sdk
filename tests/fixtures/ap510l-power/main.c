#include <assert.h>
#include <stdlib.h>
#include "am_bsp.h"
#include "nsx_power.h"

vcomp_t vcomp; mcuctrl_t mcuctrl; pwrctrl_t pwrctrl; scb_t scb;
const unsigned am_hal_gpio_pincfg_disabled = 0;
static am_hal_pwrctrl_mcu_memory_config_t memory;
static am_hal_pwrctrl_sram_memcfg_t sram;
static unsigned periph[TEST_PERIPH_COUNT], rss, bsp, cpdlp, cache, mask, events;
static unsigned fail_debug;
static nsx_power_perf_mode_t performance;
uint32_t am_hal_interrupt_master_disable(void) { unsigned old=mask; mask=1; return old; }
void am_hal_interrupt_master_set(uint32_t value) { mask=value; }
void am_bsp_low_power_init(void) { bsp++; events++; }
uint32_t am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e p) { assert(p<TEST_PERIPH_COUNT); periph[p]++; return 0; }
uint32_t am_hal_clkgen_control(unsigned op, void *arg) { (void)op; (void)arg; return 0; }
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *cfg) { memory=*cfg; return 0; }
uint32_t am_hal_pwrctrl_sram_config(am_hal_pwrctrl_sram_memcfg_t *cfg) { sram=*cfg; return 0; }
uint32_t am_hal_pwrctrl_pwrmodctl_cpdlp_config(am_hal_pwrctrl_pwrmodctl_cpdlp_t cfg) {
 assert(cfg.eRlpConfig==AM_HAL_PWRCTRL_RLP_ON); assert(!cache); cpdlp++; return 0;
}
uint32_t am_hal_cachectrl_icache_enable(void) { assert(cpdlp==1); cache++; return 0; }
uint32_t am_hal_cachectrl_dcache_enable(bool clean) { assert(clean && cache==1); cache++; return 0; }
void am_hal_rtc_osc_select(uint32_t osc) { (void)osc; }
uint32_t am_hal_rtc_osc_disable(void) { return 0; }
void am_hal_sysctrl_fpu_enable(void) {}
void am_hal_sysctrl_fpu_stacking_enable(bool lazy) { assert(lazy); }
uint32_t am_hal_pwrctrl_rss_pwroff(void) { rss++; return 0; }
void am_hal_sysctrl_sleep(unsigned mode) { (void)mode; }
uint32_t am_hal_pwrctrl_control(unsigned op, void *arg) { (void)op; (void)arg; return 0; }
void am_hal_timer_stop(uint32_t timer) { (void)timer; }
uint32_t am_hal_gpio_pinconfig(uint32_t pin, unsigned cfg) { (void)pin; (void)cfg; return 0; }
void nsx_printf(const char *fmt, ...) { (void)fmt; }
void nsx_delay_us(uint32_t delay) { (void)delay; }
uint32_t nsx_core_check_api(const nsx_core_api_t *a, const nsx_core_api_t *b, const nsx_core_api_t *c) { (void)b; (void)c; assert(a==&nsx_power_V1_0_0); return 0; }
void nsx_core_fail_loop(void) { abort(); }
uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) { performance=mode; return 0; }
uint32_t nsx_debug_before_hw_init(void) { events++; return fail_debug; }
void nsx_debug_disable(unsigned transport) { (void)transport; }

int main(int argc, char **argv) {
 assert(argc==5);
 nsx_power_config_t cfg=nsx_power_all_on;
 cfg.small_tcm=atoi(argv[1]); cfg.need_ssram=atoi(argv[2]);
 bool keep=atoi(argv[3]); unsigned failure=atoi(argv[4]);
 cfg.need_ble=cfg.need_usb=cfg.need_iom=cfg.need_uart=keep;
 cfg.spotmgr_collapse=true; cfg.need_audadc=true;
 mask=1;
 if (failure==1) {
   cfg.perf_mode=(nsx_power_perf_mode_t)99;
   assert(nsx_power_configure(&cfg)==NSX_STATUS_INVALID_CONFIG);
   assert(!events); cfg.perf_mode=NSX_POWER_PERF_LOW;
 } else if (failure==2) {
   fail_debug=73;
   assert(nsx_power_configure(&cfg)==73); assert(!bsp);
   assert(nsx_power_configure(&cfg)==NSX_STATUS_STARTUP_REJECTED);
   assert(mask==1); return 0;
 }
 assert(nsx_power_configure(&cfg)==0);
 assert(bsp==1 && cache==2 && mask==1 && performance==cfg.perf_mode);
 assert(memory.eDTCMCfg==(cfg.small_tcm ? AM_HAL_PWRCTRL_DTCM128K : AM_HAL_PWRCTRL_DTCM256K));
 assert(memory.eRetainDTCM==AM_HAL_PWRCTRL_MEMRETCFG_TCMPWDSLP_RETAIN);
 assert(memory.eNVMCfg==AM_HAL_PWRCTRL_NVM);
 assert(sram.eSRAMCfg==(cfg.need_ssram ? AM_HAL_PWRCTRL_SRAM_1P75M : AM_HAL_PWRCTRL_SRAM_NONE));
 assert(sram.eSRAMRetain==sram.eSRAMCfg);
 assert(!sram.eActiveWithMCU && !sram.eActiveWithGFX && !sram.eActiveWithDISP);
 assert(rss==!keep);
 for (unsigned p=AM_HAL_PWRCTRL_PERIPH_IOM0;p<=AM_HAL_PWRCTRL_PERIPH_USBPHY;p++) assert(periph[p]==!keep);
 assert(nsx_power_configure(&cfg)==NSX_STATUS_STARTUP_REJECTED); assert(bsp==1);
 assert(nsx_power_minimize_memory()==0);
 assert(memory.eDTCMCfg==AM_HAL_PWRCTRL_DTCM128K && memory.eNVMCfg==AM_HAL_PWRCTRL_NVM);
 assert(sram.eSRAMCfg==AM_HAL_PWRCTRL_SRAM_NONE);
 pwrctrl.MEMPWREN_b.PWRENNVM=1; nsx_power_disable_nvm(); assert(!pwrctrl.MEMPWREN_b.PWRENNVM);
 return 0;
}
