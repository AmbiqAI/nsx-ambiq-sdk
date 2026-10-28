#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "nsx_system.h"

#define FAILURE 0x41u
static unsigned calls, fallible, fail_at, performance_calls, tempco_calls;
static uint32_t mask, expected_mask;
static bool reenter;
static bool failed, peripheral[TEST_PERIPHERAL_COUNT];
static const char *trace[128];
static nsx_power_config_t config;
test_vcomp_t test_vcomp;
const am_hal_cachectrl_config_t am_hal_cachectrl_defaults = 42;
const unsigned am_hal_gpio_pincfg_output = 43;

static void event(const char *name) {
    assert(mask == expected_mask);
    assert(!failed); /* No later operation may run after an injected failure. */
    assert(calls < 128);
    trace[calls++] = name;
}
static uint32_t operation(const char *name) {
    event(name);
    if (++fallible == fail_at) { failed = true; return FAILURE; }
    return 0;
}
uint32_t am_hal_interrupt_master_disable(void) { uint32_t old=mask; mask=1; return old; }
void am_hal_interrupt_master_set(uint32_t value) { mask=value; }
void am_bsp_low_power_init(void) {
    event("bsp");
    if (reenter) {
        nsx_system_config_t nested={0};
        assert(nsx_system_init(&nested) == NSX_STATUS_STARTUP_REJECTED);
        assert(nsx_minimal_hw_init() == NSX_STATUS_STARTUP_REJECTED);
        assert(nsx_hw_init() == NSX_STATUS_STARTUP_REJECTED);
        assert(nsx_power_configure(&config) == NSX_STATUS_STARTUP_REJECTED);
    }
}
#ifdef TEST_APOLLO5_OWNERSHIP
/* Isolate wrapper ownership from the legacy peripheral/memory sequence. */
uint32_t nsx_power_platform_config(const nsx_power_config_t *cfg) {
    assert(cfg); am_bsp_low_power_init(); return operation("power-config");
}
void nsx_power_platform_deep_sleep(void) {assert(!"unexpected sleep");}
#endif
uint32_t nsx_platform_hw_init(void) { am_bsp_low_power_init(); return operation("system-hw"); }
uint32_t nsx_platform_minimal_hw_init(void) { return operation("minimal-hw"); }
uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode) { (void)mode; return operation("system-perf"); }
uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) { (void)cfg; return operation("debug"); }
uint32_t nsx_platform_spot_mgr_profile(void) { return operation("spot"); }
uint32_t nsx_cache_enable(void) { return operation("system-cache"); }
void nsx_printf(const char *fmt, ...) { (void)fmt; }
void nsx_low_power_printf(const char *fmt, ...) { (void)fmt; }
/* Core lifecycle is compiled separately with its fatal endpoint renamed. */
void nsx_core_fail_loop(void) { assert(!"power configuration entered fail loop"); abort(); }
void am_hal_sysctrl_sleep(unsigned mode) { (void)mode; assert(!"unexpected sleep"); }
void nsx_power_tempco_prepare_for_sleep(void) { assert(!"unexpected sleep preparation"); }
uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode) {
    assert(mode == config.perf_mode);
    ++performance_calls;
    return operation("performance");
}
uint32_t nsx_power_tempco_enable(void) { ++tempco_calls; return operation("tempco"); }
uint32_t am_hal_cachectrl_config(const am_hal_cachectrl_config_t *cfg) {
    assert(cfg == &am_hal_cachectrl_defaults); return operation("cache-config");
}
uint32_t am_hal_cachectrl_enable(void) { return operation("cache-enable"); }
uint32_t am_hal_mcuctrl_control(unsigned control, void *arg) {
    assert(control == AM_HAL_MCUCTRL_CONTROL_SRAM_PREFETCH);
    assert(*(uint32_t *)arg == AM_HAL_MCUCTRL_SRAM_PREFETCH_DATA);
    return operation("prefetch");
}
uint32_t am_hal_clkgen_control(unsigned control, void *arg) {
    assert(control == AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC && arg == NULL);
    return operation("rtc");
}
uint32_t am_hal_gpio_pinconfig(uint32_t pin, unsigned value) {
    assert(pin == AM_DEVICES_BLECTRLR_RESET_PIN && value == am_hal_gpio_pincfg_output);
    return operation("gpio-config");
}
uint32_t am_hal_gpio_state_write(uint32_t pin, unsigned value) {
    assert(pin == AM_DEVICES_BLECTRLR_RESET_PIN);
    assert(value == AM_HAL_GPIO_OUTPUT_CLEAR || value == AM_HAL_GPIO_OUTPUT_SET);
    return operation(value ? "gpio-set" : "gpio-clear");
}
uint32_t am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e p) {
    assert((unsigned)p < TEST_PERIPHERAL_COUNT && !peripheral[p]);
    peripheral[p] = true; return operation("peripheral");
}
uint32_t am_hal_pwrctrl_control(unsigned control, void *arg) {
    assert(arg == NULL);
    assert(control == AM_HAL_PWRCTRL_CONTROL_CRYPTO_POWERDOWN || control == AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP);
    return operation(control == AM_HAL_PWRCTRL_CONTROL_CRYPTO_POWERDOWN ? "crypto" : "xtal");
}
uint32_t am_hal_pwrctrl_sram_config(am_hal_pwrctrl_sram_memcfg_t *cfg) {
    (void)cfg; assert(!"unexpected SSRAM reduction"); return FAILURE;
}
uint32_t am_hal_pwrctrl_dsp_memory_config(unsigned dsp, am_hal_pwrctrl_dsp_memory_config_t *cfg) {
    (void)dsp; (void)cfg; assert(!"unexpected DSP memory reduction"); return FAILURE;
}
uint32_t am_hal_daxi_config(const am_hal_daxi_config_t *cfg) {
    assert(config.need_ssram && !cfg->bDaxiPassThrough && !cfg->bAgingSEnabled);
    assert(cfg->eAgingCounter == AM_HAL_DAXI_CONFIG_AGING_1024);
    assert(cfg->eNumBuf == AM_HAL_DAXI_CONFIG_NUMBUF_32 && cfg->eNumFreeBuf == AM_HAL_DAXI_CONFIG_NUMFREEBUF_3);
    return operation("daxi");
}
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *cfg) {
    (void)cfg; assert(!"unexpected DTCM reduction"); return FAILURE;
}
static void success_contract(void) {
    assert(calls && !strcmp(trace[0], "bsp") && performance_calls == 1);
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    assert(calls == 5 && !strcmp(trace[1], "performance"));
    assert(!strcmp(trace[2], "cache-config") && !strcmp(trace[3], "cache-enable") && !strcmp(trace[4], "prefetch"));
#else
    assert(!strcmp(trace[1], "cache-config") && !strcmp(trace[2], "cache-enable"));
    assert(!strcmp(trace[3], "daxi"));
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_IOS]);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_IOM0] == !config.need_iom);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_UART0] == !config.need_uart);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_DEBUG] == !config.need_itm);
    assert(test_vcomp.PWDKEY == VCOMP_PWDKEY_PWDKEY_Key);
    assert(!strcmp(trace[calls - (config.need_tempco ? 2 : 1)], "performance"));
    assert(tempco_calls == (unsigned)config.need_tempco);
#endif
}
static void all_startups_rejected(void) {
    unsigned before=calls;
    nsx_system_config_t system={0};
    assert(nsx_system_init(&system) == NSX_STATUS_STARTUP_REJECTED);
    assert(nsx_hw_init() == NSX_STATUS_STARTUP_REJECTED);
    assert(nsx_minimal_hw_init() == NSX_STATUS_STARTUP_REJECTED);
    assert(nsx_power_configure(&config) == NSX_STATUS_STARTUP_REJECTED);
    nsx_core_config_t core={.api=&nsx_core_current_version};
    assert(nsx_core_init(&core) == 0);
    assert(nsx_power_configure(&config) == NSX_STATUS_STARTUP_REJECTED);
    assert(calls == before);
}
static void ownership(const char *scenario) {
    nsx_system_config_t system={.enable_cache=true, .debug={.transport=NSX_DEBUG_ITM}};
    if (!strcmp(scenario,"preflight")) {
        system.perf_mode=(nsx_perf_mode_e)99;
        assert(nsx_system_init(&system) == NSX_STATUS_INVALID_CONFIG);
        system.perf_mode=NSX_PERF_LOW; system.debug.transport=(nsx_debug_transport_e)99;
        assert(nsx_system_init(&system) == NSX_STATUS_INVALID_CONFIG);
        assert(nsx_system_init(NULL) == NSX_STATUS_INVALID_HANDLE);
        nsx_power_config_t bad=config; bad.perf_mode=(nsx_power_perf_mode_t)99;
        assert(nsx_power_configure(&bad) == NSX_STATUS_INVALID_CONFIG);
        assert(calls == 0); system.debug.transport=NSX_DEBUG_ITM;
    }
    if (!strncmp(scenario,"masked",6)) mask=expected_mask=1;
    if (!strcmp(scenario,"system-minimal")) system.skip_bsp_init=true;
    if (!strcmp(scenario,"reentry") || !strcmp(scenario,"power-reentry")) reenter=true;
    uint32_t status;
    if (!strcmp(scenario,"hardware")) status=nsx_hw_init();
    else if (!strcmp(scenario,"minimal") || !strcmp(scenario,"minimal-failure")) {
        if (!strcmp(scenario,"minimal-failure")) fail_at=1;
        status=nsx_minimal_hw_init();
    }
    else if (!strncmp(scenario,"power-",6)) {
        if (!strcmp(scenario,"power-failure")) fail_at=1;
        if (!strcmp(scenario,"power-not-set")) config.perf_mode=NSX_POWER_PERF_NOT_SET;
        status=nsx_power_configure(&config);
    }
    else {
        if (!strncmp(scenario,"system-failure-",15)) fail_at=(unsigned)strtoul(scenario+15,NULL,10);
        if (!strcmp(scenario,"masked-failure")) fail_at=3;
        status=nsx_system_init(&system);
        if (!system.skip_bsp_init && !failed) {
            assert(!strcmp(trace[0],"bsp") && !strcmp(trace[1],"system-hw"));
            assert(!strcmp(trace[2],"system-cache") && !strcmp(trace[3],"debug"));
            assert(!strcmp(trace[4],"system-perf"));
        }
    }
    assert(status == (fail_at ? FAILURE : 0));
    if (system.skip_bsp_init || !strcmp(scenario,"minimal")) assert(!strcmp(trace[0],"minimal-hw"));
    assert(mask == expected_mask);
    all_startups_rejected();
    assert(mask == expected_mask);
}
static unsigned count_event(const char *name) {
    unsigned count=0;
    for (unsigned i=0;i<calls;++i) if (!strcmp(trace[i],name)) ++count;
    return count;
}
static void peripheral_policy(const char *scenario) {
    struct field { const char *name; size_t offset; };
    static const struct field fields[] = {
#define FIELD(name) {#name, offsetof(nsx_power_config_t, name)}
        FIELD(need_usb), FIELD(need_iom), FIELD(need_uart), FIELD(need_audadc),
        FIELD(need_crypto), FIELD(need_xtal), FIELD(need_itm), FIELD(need_ble),
        FIELD(spotmgr_collapse),
#undef FIELD
    };
    if (!strcmp(scenario,"preset-all_on")) {
        config=nsx_power_all_on;
        assert(config.need_usb && config.need_ble && config.need_iom && config.need_uart);
        assert(config.need_audadc && config.need_itm && config.need_xtal);
        assert(!config.need_crypto && !config.need_tempco);
    } else if (!strcmp(scenario,"preset-minimal") || !strcmp(scenario,"preset-audio")) {
        bool audio=!strcmp(scenario,"preset-audio");
        config=audio ? nsx_power_audio : nsx_power_minimal;
        assert(config.need_audadc == audio);
        assert(!config.need_usb && !config.need_ble && !config.need_iom && !config.need_uart);
        assert(!config.need_crypto && !config.need_itm && !config.need_xtal && !config.need_tempco);
    }
    else {
        unsigned selected=0; char direction, name[32];
        assert(sscanf(scenario,"%31[^-]-%c",name,&direction) == 2);
        while (selected < sizeof fields/sizeof fields[0] && strcmp(name,fields[selected].name)) ++selected;
        assert(selected < sizeof fields/sizeof fields[0]);
        assert(direction == '0' || direction == '1');
        bool requested=direction == '1';
        config=nsx_power_minimal;
        config.need_ssram=true;
        for (unsigned i=0;i<sizeof fields/sizeof fields[0];++i)
            *(bool *)((char *)&config+fields[i].offset) = i == selected ? requested : !requested;
    }
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
    if (!config.need_ssram) {
        assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_CONFIG);
        assert(calls == 0);
        // Test the preset's peripheral policy with an explicit full-memory copy.
        config.need_ssram = true;
    }
#endif
    assert(nsx_power_configure(&config) == 0);
    success_contract();
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    for (unsigned p=0;p<TEST_PERIPHERAL_COUNT;++p) assert(!peripheral[p]);
    assert(count_event("crypto") == 0 && count_event("xtal") == 0);
    assert(count_event("gpio-config") == 0 && count_event("gpio-set") == 0);
#else
    for (unsigned p=AM_HAL_PWRCTRL_PERIPH_IOM0;p<=AM_HAL_PWRCTRL_PERIPH_IOM7;++p)
        assert(peripheral[p] == !config.need_iom);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_IOS]);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_UART0] == !config.need_uart);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_UART3] == !config.need_uart);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_UART1] && peripheral[AM_HAL_PWRCTRL_PERIPH_UART2]);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_DEBUG] == !config.need_itm);
    assert(count_event("crypto") == (unsigned)!config.need_crypto);
    assert(count_event("xtal") == (unsigned)!config.need_xtal);
    assert(count_event("rtc") == 1);
    assert(count_event("gpio-config") == (unsigned)!config.need_ble);
    assert(count_event("gpio-clear") == 2u * !config.need_ble);
    assert(count_event("gpio-set") == (unsigned)!config.need_ble);
    /* The mandatory BSP baseline is separate from these optional adapter writes. */
    assert(count_event("bsp") == 1);
#if defined(AM_PART_APOLLO4L)
    assert(!peripheral[AM_HAL_PWRCTRL_PERIPH_USB] && !peripheral[AM_HAL_PWRCTRL_PERIPH_USBPHY]);
    assert(!peripheral[AM_HAL_PWRCTRL_PERIPH_AUDADC]);
#else
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_USB] == !config.need_usb);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_USBPHY] == !config.need_usb);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_AUDADC] == !config.need_audadc);
#endif
#endif
    assert(count_event("spot") == 0);
    all_startups_rejected();
}
static void memory_preflight(const char *scenario) {
    unsigned small, ssram;
    assert(sscanf(scenario, "%u-%u", &small, &ssram) == 2);
    assert(small <= 1 && ssram <= 1);
    config.small_tcm = small;
    config.need_ssram = ssram;
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
    if (small || !ssram) {
        for (expected_mask=0; expected_mask<2; ++expected_mask) {
            mask=expected_mask;
            assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_CONFIG);
            assert(calls == 0 && mask == expected_mask);
            // Direct backend callers must also be rejected before BSP effects.
            extern uint32_t nsx_power_platform_config(const nsx_power_config_t *);
            assert(nsx_power_platform_config(&config) == NSX_STATUS_INVALID_CONFIG);
            assert(calls == 0 && mask == expected_mask);
        }
        mask=expected_mask=0;
        config.small_tcm = false;
        config.need_ssram = true;
    }
#endif
    assert(nsx_power_configure(&config) == 0);
    success_contract();
    all_startups_rejected();
}
int main(int argc, char **argv) {
    assert(argc == 2 || argc == 3);
    const char *scenario = argv[1];
    config = nsx_power_minimal;
    if (!strncmp(scenario,"memory-",7)) { memory_preflight(scenario+7); return 0; }
    // Non-memory scenarios retain memory so they reach their intended boundary.
    config.need_ssram = true;
    if (!strncmp(scenario,"policy-",7)) { peripheral_policy(scenario+7); return 0; }
    if (!strncmp(scenario,"ownership-",10)) { ownership(scenario+10); return 0; }
    if (argc == 3) fail_at=(unsigned)strtoul(argv[2],NULL,10);
    if (strstr(scenario, "keep")) {
        config.need_ssram = config.need_iom = config.need_uart = true;
        config.need_crypto = config.need_xtal = config.need_ble = config.need_itm = config.need_audadc = true;
    }
    if (strstr(scenario, "tempco")) config.need_tempco = true;
    if (strcmp(scenario, "no-core")) {
        nsx_core_config_t core = {.api = &nsx_core_current_version};
        assert(nsx_core_init(&core) == 0);
    }
    if (!strcmp(scenario, "null")) {
        assert(nsx_power_configure(NULL) == NSX_STATUS_INVALID_HANDLE); assert(calls == 0); return 0;
    }
    if (!strcmp(scenario, "null-api")) {
        config.api = NULL;
        assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_VERSION); assert(calls == 0); return 0;
    }
    if (!strcmp(scenario, "bad-api")) {
        nsx_core_api_t api = nsx_power_current_version; api.apiId++;
        config.api = &api;
        assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_VERSION); assert(calls == 0); return 0;
    }
    if (!strcmp(scenario, "bad-version")) {
        nsx_core_api_t api = nsx_power_current_version; api.version.major++;
        config.api = &api;
        assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_VERSION); assert(calls == 0); return 0;
    }
    if (!strcmp(scenario, "invalid-mode")) {
        const nsx_power_perf_mode_t modes[] = {NSX_POWER_PERF_NOT_SET, (nsx_power_perf_mode_t)3};
        for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; ++i) {
            config.perf_mode = modes[i];
            assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_CONFIG); assert(calls == 0);
        }
        return 0;
    }
    if (!strcmp(scenario, "unsupported-tempco")) {
        assert(nsx_power_configure(&config) == NSX_STATUS_INVALID_CONFIG); assert(calls == 0); return 0;
    }
    uint32_t status=nsx_power_configure(&config);
    if (fail_at) {
        assert(status == FAILURE && failed && fallible == fail_at);
    } else {
        assert(status == NSX_STATUS_SUCCESS);
        success_contract();
        if (!strncmp(scenario,"fail-",5)) printf("%u\n",fallible);
    }
    all_startups_rejected();
    return 0;
}

uint32_t nsx_platform_debug_disable(nsx_debug_transport_e transport) {(void)transport; return 0;}
