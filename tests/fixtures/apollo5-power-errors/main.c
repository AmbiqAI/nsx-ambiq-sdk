#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "am_hal_spotmgr.h"
#include "nsx_power.h"

vcomp_t vcomp; mcuctrl_t mcuctrl; pwrctrl_t pwrctrl; scb_t scb;
const unsigned am_hal_gpio_pincfg_disabled=0;
static unsigned calls, fail_at, failed, mask, memory_written;
static nsx_power_config_t config;
/* Every later HAL/runtime operation would fail this assertion after injection. */
static uint32_t step(const char *name, bool fallible) {
    assert(!failed);
    printf("%s\n", name);
    calls++;
    if (calls==fail_at) { assert(fallible); failed=1; return 0x200u+calls; }
    return 0;
}
uint32_t am_hal_interrupt_master_disable(void) { unsigned old=mask; mask=1; return old; }
void am_hal_interrupt_master_set(uint32_t value) { mask=value; }
void am_bsp_low_power_init(void) { step("bsp", false); }
uint32_t am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e p) {
    char name[32]; snprintf(name,sizeof(name),"periph-%u",(unsigned)p); return step(name,true);
}
uint32_t am_hal_clkgen_control(unsigned op, void *arg) { (void)op; (void)arg; return step("clkgen",true); }
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *cfg) {
    assert(cfg->eDTCMCfg!=0); memory_written=1; return step("mcu-memory",true);
}
uint32_t am_hal_pwrctrl_sram_config(am_hal_pwrctrl_sram_memcfg_t *cfg) {
    assert(memory_written); assert(cfg->eSRAMCfg==cfg->eSRAMRetain); return step("sram",true);
}
uint32_t am_hal_pwrctrl_pwrmodctl_cpdlp_config(am_hal_pwrctrl_pwrmodctl_cpdlp_t cfg) { assert(cfg.eRlpConfig==AM_HAL_PWRCTRL_RLP_ON); return step("cpdlp",true); }
uint32_t am_hal_cachectrl_icache_enable(void) { return step("icache",true); }
uint32_t am_hal_cachectrl_dcache_enable(bool clean) { assert(clean); return step("dcache",true); }
void am_hal_spotmgr_profile_set(am_hal_spotmgr_profile_t *cfg) { assert(cfg->PROFILE_b.COLLAPSESTMANDSTMP); step("spotmgr",false); }
void am_hal_rtc_osc_select(uint32_t osc) { (void)osc; step("rtc-select",false); }
uint32_t am_hal_rtc_osc_disable(void) { return step("rtc-disable",true); }
void am_hal_sysctrl_fpu_enable(void) { step("fpu",false); }
void am_hal_sysctrl_fpu_stacking_enable(bool lazy) { assert(lazy); step("fpu-stacking",false); }
uint32_t am_hal_pwrctrl_rss_pwroff(void) { return step("rss",true); }
void am_hal_sysctrl_sleep(unsigned mode) { (void)mode; step("sleep",false); }
uint32_t am_hal_pwrctrl_control(unsigned op, void *arg) { (void)op; (void)arg; return step("control",true); }
void am_hal_timer_stop(uint32_t timer) { (void)timer; step("timer",false); }
uint32_t am_hal_gpio_pinconfig(uint32_t pin, unsigned cfg) { (void)pin; (void)cfg; return step("gpio",true); }
void nsx_printf(const char *fmt, ...) { (void)fmt; step("printf",false); }
void nsx_delay_us(uint32_t delay) { (void)delay; step("delay",false); }
void nsx_core_fail_loop(void) { abort(); }
uint32_t nsx_core_check_api(const nsx_core_api_t *a,const nsx_core_api_t *b,const nsx_core_api_t *c) { (void)b; (void)c; assert(a==&nsx_power_V1_0_0); return 0; }
uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) { assert(mode==config.perf_mode); return step("performance",true); }
uint32_t nsx_debug_before_hw_init(void) { return step("debug-release",true); }
void nsx_debug_disable(unsigned transport) { (void)transport; step("debug-disable",false); }

int main(int argc,char **argv) {
    assert(argc==6);
    bool keep=atoi(argv[1]); fail_at=atoi(argv[2]); mask=atoi(argv[3]);
    config=nsx_power_all_on;
    config.small_tcm=atoi(argv[4]); config.need_ssram=atoi(argv[5]);
    config.need_ble=config.need_usb=config.need_iom=config.need_uart=keep;
    config.need_itm=config.need_crypto=config.need_audadc=config.need_xtal=keep;
    config.need_tempco=true; /* A post-performance action must also be skipped on error. */
    unsigned original_mask=mask;
    uint32_t result=nsx_power_configure(&config);
    assert(mask==original_mask);
    if (fail_at) {
        assert(failed && calls==fail_at && result==0x200u+fail_at);
        assert(mcuctrl.MRAMCRYPTOPWRCTRL_b.CRYPTOCLKGATEN==0);
    } else {
        assert(result==0 && mcuctrl.MRAMCRYPTOPWRCTRL_b.CRYPTOCLKGATEN==1);
    }
    /* All failed attempts, including partially applied memory changes, consume ownership. */
    assert(nsx_power_configure(&config)==NSX_STATUS_STARTUP_REJECTED);
    assert(mask==original_mask);
    return 0;
}
