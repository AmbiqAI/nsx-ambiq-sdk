/* Execute actual coordinator/backends with controlled HAL outcomes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "am_bsp.h"
#include "am_util_stdio.h"
#include "nsx_system.h"

enum { CORE = 1, BSP, CACHE_CONFIG, CACHE_ENABLE, CPDLP, SPOT, FPU, STACK,
       CACHE, OTP_ON, CRYPTO_ON, DCU_GET, DCU_UPDATE, CRYPTO_OFF, OTP_OFF,
       DEBUG, TPIU, GPIO, PRINT, PERF, PROFILE, ITM_OFF, PIN_HIGH, PIN_OUTPUT, TPIU_OFF, DETACH, PIN_DEFAULT, SWO_PREP };
static unsigned calls[64], count, failure, secondary_failure, irq_mask;
static unsigned selected_mode, print_calls;
uint32_t SystemCoreClock = 96000000;
static unsigned failure_window = 1, power_window;
static unsigned step(unsigned id) {
    assert(count < sizeof(calls) / sizeof(calls[0]));
    calls[count++] = id;
    unsigned window = (id >= OTP_ON && id <= OTP_OFF) ? power_window : failure_window;
    return (id == failure || id == secondary_failure) && window == failure_window ? 0x40 + id : 0;
}
static unsigned seen(unsigned id) {
    unsigned n = 0;
    for (unsigned i = 0; i < count; ++i) n += calls[i] == id;
    return n;
}
power_t power;
crypto_t crypto = {.HOSTCCISIDLE_b = {1}};
itm_t itm;
static unsigned unlocked;
const am_hal_cachectrl_config_t am_hal_cachectrl_defaults = 0;
uint32_t __real_nsx_core_init(nsx_core_config_t *cfg);
uint32_t __wrap_nsx_core_init(nsx_core_config_t *cfg) {
    unsigned status=step(CORE);
    return status ? status : __real_nsx_core_init(cfg);
}
void am_util_stdio_vprintf(const char *fmt,va_list args) {(void)fmt; (void)args; ++print_calls;}
void am_util_delay_us(unsigned us) {(void)us;}
uint32_t nsx_cache_enable(void) {return step(CACHE);}
uint32_t am_hal_interrupt_master_disable(void) {unsigned old=irq_mask; irq_mask=1; ++power_window; return old;}
void am_hal_interrupt_master_enable(void) {irq_mask=0;}
void am_hal_interrupt_master_set(uint32_t mask) {irq_mask=mask;}
void am_bsp_low_power_init(void) {(void)step(BSP);}
uint32_t am_hal_cachectrl_config(const am_hal_cachectrl_config_t *cfg) {(void)cfg; return step(CACHE_CONFIG);}
uint32_t am_hal_cachectrl_enable(void) {return step(CACHE_ENABLE);}
void am_hal_sysctrl_fpu_enable(void) {(void)step(FPU);}
void am_hal_sysctrl_fpu_stacking_enable(bool lazy) {assert(lazy); (void)step(STACK);}
uint32_t am_hal_pwrctrl_mcu_mode_select(unsigned mode) {
    assert(mode >= 10 && mode <= 13); selected_mode=mode; return step(PERF);
}
#if (defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P))
void am_bsp_itm_printf_enable(void) {(void)step(DEBUG);}
void am_bsp_uart_printf_enable(void) {(void)step(DEBUG);}
#else
uint32_t am_bsp_itm_printf_enable(void) {return step(DEBUG);}
uint32_t am_bsp_uart_printf_enable(void) {return step(DEBUG);}
#endif
uint32_t am_hal_pwrctrl_periph_enable(unsigned domain) {
    assert(irq_mask == 1);
    /* Model a partial hardware change even when enable returns an error. */
    if (domain == AM_HAL_PWRCTRL_PERIPH_OTP) power.DEVPWRSTATUS_b.PWRSTOTP=1;
    else power.DEVPWRSTATUS_b.PWRSTCRYPTO=1;
    return step(domain == AM_HAL_PWRCTRL_PERIPH_OTP ? OTP_ON : CRYPTO_ON);
}
uint32_t am_hal_pwrctrl_periph_disable(unsigned domain) {
    assert(irq_mask == 1);
    unsigned status=step(domain == AM_HAL_PWRCTRL_PERIPH_OTP ? OTP_OFF : CRYPTO_OFF);
    if (!status) {
        if (domain == AM_HAL_PWRCTRL_PERIPH_OTP) power.DEVPWRSTATUS_b.PWRSTOTP=0;
        else power.DEVPWRSTATUS_b.PWRSTCRYPTO=0;
    }
    return status;
}
uint32_t am_hal_dcu_get(uint32_t *value) {
    assert(irq_mask == 1); *value=unlocked ? 63 : 0; return step(DCU_GET);
}
uint32_t am_hal_dcu_update(bool enable, uint32_t mask) {
    assert(irq_mask == 1 && enable && mask == 63); return step(DCU_UPDATE);
}
uint32_t am_hal_pwrctrl_pwrmodctl_cpdlp_config(am_hal_pwrctrl_pwrmodctl_cpdlp_t cfg) {
    assert(cfg.eRlpConfig == 1 && cfg.eElpConfig == 1 && cfg.eClpConfig == 1); return step(CPDLP);
}
uint32_t am_hal_spotmgr_init(void) {return step(SPOT);}
void am_hal_spotmgr_profile_set(am_hal_spotmgr_profile_t *profile) {
    assert(profile->PROFILE_b.COLLAPSESTMANDSTMP); (void)step(PROFILE);
}
uint32_t am_hal_debug_enable(void) {return step(DEBUG);}
uint32_t am_hal_dcu_swo_enable(void) {
    assert(irq_mask == 1 && power.DEVPWRSTATUS_b.PWRSTOTP && power.DEVPWRSTATUS_b.PWRSTCRYPTO);
    return step(SWO_PREP);
}
uint32_t am_hal_tpiu_enable(uint32_t baud) {assert(baud == 1000000); return step(TPIU);}
uint32_t am_hal_itm_enable(void) {return step(DEBUG);}
uint32_t am_hal_tpiu_config(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e) {
    assert(a == 0 && b == 0 && c == 1 && d == 1 && e == 95); return step(TPIU);
}
uint32_t am_hal_gpio_pinconfig(unsigned pin,unsigned cfg) {assert(pin == 28); return step(cfg == 1 ? GPIO : cfg == 2 ? PIN_OUTPUT : PIN_DEFAULT);}
void am_hal_itm_print(char *s) {(void)s;}
void am_util_stdio_printf_init(am_util_stdio_print_char_t fn) {assert(fn == NULL || fn == am_hal_itm_print); (void)step(fn ? PRINT : DETACH);}

#include "contract_cases.h"

int main(int argc, char **argv) {
    assert(argc >= 2);
    if(!strcmp(argv[1], "contract")) {
        assert(argc == 3); contract_startup(argv[2]); puts("PASS"); return 0;
    }
    if(!strcmp(argv[1], "performance")) {
        assert(argc == 5);
        contract_performance((unsigned)atoi(argv[2]),(unsigned)atoi(argv[3]),atoi(argv[4]) != 0);
        puts("PASS"); return 0;
    }
    nsx_system_config_t cfg = {.perf_mode=NSX_PERF_LOW, .enable_cache=true,
                              .debug={.transport=NSX_DEBUG_ITM}, .spot_mgr_profile=true};
    unsigned status;
    if (!strcmp(argv[1], "invalid")) {
        for (unsigned i=0; i<2; ++i) {
            nsx_perf_mode_e mode=(nsx_perf_mode_e)(i ? -1 : 99);
            nsx_debug_config_t debug={.transport=(nsx_debug_transport_e)(i ? -1 : 99)};
            assert(nsx_set_perf_mode(mode) == NSX_STATUS_INVALID_CONFIG);
            assert(nsx_debug_init(&debug) == NSX_STATUS_INVALID_CONFIG);
            cfg.perf_mode=mode;
            assert(nsx_system_init(&cfg) == NSX_STATUS_INVALID_CONFIG);
            cfg.perf_mode=NSX_PERF_LOW;
            cfg.debug=debug;
            assert(nsx_system_init(&cfg) == NSX_STATUS_INVALID_CONFIG);
            cfg.debug.transport=NSX_DEBUG_ITM;
        }
        assert(nsx_system_init(NULL) == NSX_STATUS_INVALID_HANDLE);
        assert(nsx_debug_init(NULL) == 0);
        nsx_debug_config_t none={.transport=NSX_DEBUG_NONE};
        assert(nsx_debug_init(&none) == 0 && count == 0);
        assert(nsx_system_init(&cfg) == 0); /* preflight did not claim startup */
    } else if (!strcmp(argv[1], "minimal")) {
        assert(argc == 3); failure=(unsigned)atoi(argv[2]);
        cfg.skip_bsp_init=true;
        status=nsx_system_init(&cfg);
        assert(status == (failure ? 0x40+failure : 0));
        if (failure) {
            assert(calls[count-1] == failure && !seen(PERF) && !seen(DEBUG));
#if (defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
            unsigned previous=count;
            assert(nsx_system_init(&cfg) == NSX_STATUS_STARTUP_REJECTED && count == previous);
#endif
        } else {
            assert(seen(FPU) == 1 && seen(STACK) == 1 && !seen(BSP));
        }
    } else if (!strcmp(argv[1], "debug")) {
        assert(argc == 7 || argc == 8);
        if (argc == 8) failure_window=(unsigned)atoi(argv[7]);
        failure=(unsigned)atoi(argv[2]); secondary_failure=(unsigned)atoi(argv[3]);
        unsigned domains=(unsigned)atoi(argv[4]), masked=(unsigned)atoi(argv[5]);
        unlocked=(unsigned)atoi(argv[6]); irq_mask=masked;
        power.DEVPWRSTATUS_b.PWRSTOTP=domains & 1;
        power.DEVPWRSTATUS_b.PWRSTCRYPTO=(domains >> 1) & 1;
        status=nsx_system_init(&cfg);
        assert(status == (failure ? 0x40+failure : 0) && irq_mask == masked);
        if (failure) assert(!seen(PERF) && !seen(PROFILE));
        unsigned windows = 1;
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
        if (!(failure_window == 1 && failure >= OTP_ON && failure <= OTP_OFF)) windows=2;
#endif
        if (domains & 1) assert(!seen(OTP_ON) && !seen(OTP_OFF) && power.DEVPWRSTATUS_b.PWRSTOTP);
        else assert(seen(OTP_ON) == windows && seen(OTP_OFF) == windows);
        if (domains & 2) assert(!seen(CRYPTO_ON) && !seen(CRYPTO_OFF) && power.DEVPWRSTATUS_b.PWRSTCRYPTO);
        else {
            unsigned crypto_windows = windows - (failure == OTP_ON ? 1 : 0);
            assert(seen(CRYPTO_ON) == crypto_windows && seen(CRYPTO_OFF) == crypto_windows);
        }
        if (failure >= OTP_ON && failure <= OTP_OFF) assert(!seen(DEBUG) && !seen(TPIU));
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
        if (failure == SWO_PREP) assert(!seen(DEBUG) && !seen(TPIU));
        if (failure == TPIU) assert(!seen(DEBUG));
#else
        if (failure == DEBUG) assert(!seen(TPIU));
#endif
        if (failure == DEBUG || failure == TPIU) assert(!seen(GPIO) && !seen(PRINT));
        if (failure == GPIO) assert(!seen(PRINT));
        if (unlocked) assert(!seen(DCU_UPDATE));
    } else if (!strcmp(argv[1], "success")) {
        assert(nsx_system_init(&cfg) == 0);
        assert(seen(CORE) == 1 && seen(BSP) == 1 && seen(CACHE) == 1 && seen(DEBUG) == 1);
        unsigned debug_pos=0, perf_pos=0;
        for (unsigned i=0; i<count; ++i) {if(calls[i] == DEBUG) debug_pos=i; if(calls[i] == PERF) perf_pos=i;}
#if !(defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P))
        assert(perf_pos > debug_pos);
#else
        (void)debug_pos; (void)perf_pos;
#endif
    } else if (!strcmp(argv[1], "disable")) {
        assert(argc == 3);
        assert(nsx_debug_init(&cfg.debug) == 0);
        count=0; failure=(unsigned)atoi(argv[2]);
        extern void nsx_debug_disable(nsx_debug_transport_e);
        nsx_debug_disable(NSX_DEBUG_ITM);
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
        const unsigned expected[]={ITM_OFF, TPIU_OFF, DETACH, PIN_DEFAULT};
#else
        const unsigned expected[]={ITM_OFF, PIN_HIGH, PIN_OUTPUT, TPIU_OFF, DETACH, PIN_DEFAULT};
#endif
        unsigned n=0;
        do {assert(n < count && calls[n] == expected[n]); ++n;}
        while (n < sizeof(expected)/sizeof(expected[0]) && expected[n-1] != failure);
        assert(count == n);
        if (failure) assert(nsx_debug_init(&cfg.debug) == 0x40+failure);
        else assert(nsx_debug_init(&cfg.debug) == 0);
    } else return 2;
    puts("PASS");
    return 0;
}

void am_bsp_itm_printf_disable(void) {}
void am_bsp_uart_printf_disable(void) {}
uint32_t am_hal_itm_disable(void) {return step(ITM_OFF);}
uint32_t am_hal_tpiu_disable(void) {return step(TPIU_OFF);}
uint32_t am_hal_gpio_state_write(unsigned pin, unsigned state) {assert(pin == 28 && state == 1); return step(PIN_HIGH);}
