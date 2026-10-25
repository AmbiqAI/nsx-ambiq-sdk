#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"

#define FAILURE 0x41u
static unsigned calls, fallible, fail_at, performance_calls, tempco_calls;
static bool failed, peripheral[TEST_PERIPHERAL_COUNT];
static const char *trace[128];
static nsx_power_config_t config;
test_vcomp_t test_vcomp;
const am_hal_cachectrl_config_t am_hal_cachectrl_defaults = 42;
const unsigned am_hal_gpio_pincfg_output = 43;

static void event(const char *name) {
    assert(!failed); /* No later operation may run after an injected failure. */
    assert(calls < 128);
    trace[calls++] = name;
}
static uint32_t operation(const char *name) {
    event(name);
    if (++fallible == fail_at) { failed = true; return FAILURE; }
    return 0;
}
void am_bsp_low_power_init(void) { event("bsp"); }
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
    assert(!config.need_ssram && cfg->eSRAMCfg == AM_HAL_PWRCTRL_SRAM_NONE);
    assert(cfg->eActiveWithMCU == 0 && cfg->eActiveWithDSP == 0 && cfg->eSRAMRetain == 0);
    return operation("sram");
}
uint32_t am_hal_pwrctrl_dsp_memory_config(unsigned dsp, am_hal_pwrctrl_dsp_memory_config_t *cfg) {
    assert(dsp == AM_HAL_DSP0 && !cfg->bEnableICache && !cfg->bRetainCache);
    assert(!cfg->bEnableRAM && !cfg->bActiveRAM && !cfg->bRetainRAM);
    return operation("dsp-memory");
}
uint32_t am_hal_daxi_config(const am_hal_daxi_config_t *cfg) {
    assert(config.need_ssram && !cfg->bDaxiPassThrough && !cfg->bAgingSEnabled);
    assert(cfg->eAgingCounter == AM_HAL_DAXI_CONFIG_AGING_1024);
    assert(cfg->eNumBuf == AM_HAL_DAXI_CONFIG_NUMBUF_32 && cfg->eNumFreeBuf == AM_HAL_DAXI_CONFIG_NUMFREEBUF_3);
    return operation("daxi");
}
uint32_t am_hal_pwrctrl_mcu_memory_config(am_hal_pwrctrl_mcu_memory_config_t *cfg) {
    assert(config.small_tcm && cfg->eCacheCfg == AM_HAL_PWRCTRL_CACHE_ALL && cfg->bRetainCache);
    assert(cfg->eDTCMCfg == AM_HAL_PWRCTRL_DTCM_128K && cfg->eRetainDTCM == AM_HAL_PWRCTRL_DTCM_128K);
    assert(cfg->bEnableNVM0 && !cfg->bRetainNVM0); return operation("mcu-memory");
}
static void reset_trace(void) {
    calls = fallible = performance_calls = tempco_calls = 0;
    failed = false; memset(peripheral, 0, sizeof peripheral); test_vcomp.PWDKEY = 0;
}
static void success_contract(void) {
    assert(calls && !strcmp(trace[0], "bsp") && performance_calls == 1);
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    assert(calls == 5 && !strcmp(trace[1], "performance"));
    assert(!strcmp(trace[2], "cache-config") && !strcmp(trace[3], "cache-enable") && !strcmp(trace[4], "prefetch"));
#else
    assert(!strcmp(trace[1], "cache-config") && !strcmp(trace[2], "cache-enable"));
    assert(!strcmp(trace[3], config.need_ssram ? "daxi" : "sram"));
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_IOS]);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_IOM0] == !config.need_iom);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_UART0] == !config.need_uart);
    assert(peripheral[AM_HAL_PWRCTRL_PERIPH_DEBUG] == !config.need_itm);
    assert(test_vcomp.PWDKEY == VCOMP_PWDKEY_PWDKEY_Key);
    assert(!strcmp(trace[calls - (config.need_tempco ? 2 : 1)], "performance"));
    assert(tempco_calls == (unsigned)config.need_tempco);
#endif
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *scenario = argv[1];
    config = nsx_power_minimal;
    if (strstr(scenario, "keep")) {
        config.need_ssram = config.small_tcm = config.need_iom = config.need_uart = true;
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
    assert(nsx_power_configure(&config) == NSX_STATUS_SUCCESS);
    success_contract();
    unsigned operations = fallible;
    if (!strncmp(scenario, "fail-", 5)) {
        for (unsigned i = 1; i <= operations; ++i) {
            reset_trace(); fail_at = i;
            assert(nsx_power_configure(&config) == FAILURE);
            assert(failed && fallible == i);
        }
    } else {
        reset_trace(); assert(nsx_power_configure(&config) == 0); success_contract();
    }
    return 0;
}
