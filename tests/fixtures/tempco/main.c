#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include "nsx_interrupt.h"
#include "nsx_timer.h"
#include "nsx_power_tempco.c"

#define FAIL 0x41u
#define CLEAN_FAIL 0x42u
static unsigned step, fail_step, allocations, releases, operations;
static bool allocated, powered, enabled, running, irq_enabled, cleanup;
static bool delivering;
static bool bad_trim, deinit_poison, trim_read_failure;
static const char *cleanup_failure;
static nsx_irq_handler_t handler;
static void *context;
static int handle;
test_adc_t test_adc;
const nsx_core_api_t nsx_interrupt_current_version = {0};
extern nsx_timer_config_t *nsx_timer_config[];

static uint32_t op(const char *name) {
    ++operations;
    /* Even synchronous delivery during setup/release must not use ADC. */
    if (!s_nsx_power_tempco.initialized && handler && !delivering) {
        delivering = true;
        handler(context);
        delivering = false;
    }
    if (cleanup_failure && !strcmp(cleanup_failure, name)) return CLEAN_FAIL;
    if (!cleanup && ++step == fail_step) { cleanup = true; return FAIL; }
    return 0;
}
void nsx_printf(const char *format, ...) { (void)format; }
void nsx_low_power_printf(const char *format, ...) { (void)format; }
uint32_t nsx_core_check_api(const nsx_core_api_t *a,const nsx_core_api_t *b,const nsx_core_api_t *c) {
    (void)b; (void)c; assert(a); return 0;
}
uint32_t am_hal_mram_info_read(uint32_t n,uint32_t o,uint32_t count,uint32_t *v) {
    assert(n == 1 && o == 6 && count == 1); *v = bad_trim ? 5 : 6; return trim_read_failure ? FAIL : 0;
}
uint32_t am_hal_adc_initialize(uint32_t n,void **h) {
    assert(n == 0 && !allocated); uint32_t s = op("allocate");
    if (!s) { allocated = true; ++allocations; *h = &handle; } return s;
}
uint32_t am_hal_pwrctrl_periph_enable(int p) {
    assert(p == AM_HAL_PWRCTRL_PERIPH_ADC && allocated);
    uint32_t s = op("power-on"); if (!s) powered = true; return s;
}
uint32_t am_hal_pwrctrl_periph_disable(int p) {
    assert(p == AM_HAL_PWRCTRL_PERIPH_ADC && allocated && !enabled);
    uint32_t s = op("power-off"); if (!s) powered = false; return s;
}
uint32_t am_hal_adc_power_control(void *h,int mode,bool retain) {
    assert(h == &handle && allocated);
    if (mode == AM_HAL_SYSCTRL_WAKE) {
        /* Old implementation powered the domain internally, too. */
        uint32_t s = op("wake"); if (!s) powered = true; return s;
    }
    assert(mode == AM_HAL_SYSCTRL_DEEPSLEEP && retain && powered); powered = false; return 0;
}
uint32_t am_hal_adc_configure(void *h,am_hal_adc_config_t *cfg) {
    assert(h == &handle && allocated && powered && cfg->eTrigger == AM_HAL_ADC_TRIGSEL_SOFTWARE);
    return op("configure");
}
uint32_t am_hal_pwrctrl_tempco_init(void *h,uint32_t slot) {
    assert(h == &handle && allocated && powered && slot == 5); return op("tempco");
}
uint32_t am_hal_adc_enable(void *h) {
    assert(h == &handle && allocated && powered); uint32_t s = op("enable");
    if (!s) enabled = true;
    return s;
}
uint32_t am_hal_adc_disable(void *h) {
    assert(h == &handle && allocated && powered); uint32_t s = op("disable");
    if (!s) enabled = false;
    return s;
}
uint32_t am_hal_adc_deinitialize(void *h) {
    assert(h == &handle && allocated && !powered && !enabled);
    uint32_t s = op("deallocate");
    if (!s || deinit_poison) { allocated = false; ++releases; } return s;
}
uint32_t am_hal_timer_default_config_set(am_hal_timer_config_t *c) { memset(c,0,sizeof *c); return op("timer-default"); }
uint32_t am_hal_timer_config(uint32_t n,am_hal_timer_config_t *c) {
    assert(n == NSX_TIMER_TEMPCO && c->eInputClock == AM_HAL_TIMER_CLOCK_HFRC_DIV16);
    return op("timer-config");
}
uint32_t am_hal_timer_clear(uint32_t n) { assert(n == NSX_TIMER_TEMPCO); return op("timer-clear"); }
uint32_t am_hal_timer_start(uint32_t n) {
    assert(n == NSX_TIMER_TEMPCO); uint32_t s = op("timer-start"); if (!s) running = true; return s;
}
uint32_t am_hal_timer_stop(uint32_t n) {
    assert(n == NSX_TIMER_TEMPCO); uint32_t s = op("timer-stop"); if (!s) running = false; return s;
}
uint32_t am_hal_timer_interrupt_clear(uint32_t m) { assert(m == (1u << NSX_TIMER_TEMPCO)); return op("timer-irq-clear"); }
uint32_t am_hal_timer_interrupt_enable(uint32_t m) { (void)m; return op("timer-irq-enable"); }
uint32_t am_hal_timer_interrupt_disable(uint32_t m) { (void)m; return op("timer-irq-disable"); }
uint32_t am_hal_timer_read(uint32_t n) { (void)n; return 0; }
uint32_t nsx_irq_register(const nsx_irq_config_t *c) {
    assert(c->irqn == TIMER4_IRQn); uint32_t s = op("register");
    if (!s) { handler = c->handler; context = c->ctx; irq_enabled = true; handler(context); } return s;
}
uint32_t nsx_irq_unregister(IRQn_Type n) {
    assert(n == TIMER4_IRQn); uint32_t s = op("unregister");
    if (!s) { irq_enabled = false; handler = NULL; context = NULL; } return s;
}
uint32_t nsx_irq_clear_pending(IRQn_Type n) { assert(n == TIMER4_IRQn); return op("pending-clear"); }
uint32_t am_hal_pwrctrl_tempco_sample_handler(uint32_t n,am_hal_adc_sample_t *s) { (void)n;(void)s; assert(!"unexpected capture"); return 0; }
uint32_t am_hal_adc_sw_trigger(void *h) { (void)h; assert(!"unexpected capture"); return 0; }
uint32_t am_hal_daxi_control(int c,void *p) { (void)c;(void)p; return 0; }
uint32_t am_hal_adc_samples_read(void *h,bool b,void *p,uint32_t *n,am_hal_adc_sample_t *s) { (void)h;(void)b;(void)p;(void)n;(void)s; return 0; }
void am_hal_delay_us(uint32_t t) { (void)t; }

static void clean(void) {
    assert(!allocated && !powered && !enabled && !running && !irq_enabled);
    assert(!s_nsx_power_tempco.initialized && s_nsx_power_tempco.adc_handle == NULL);
    assert(nsx_timer_config[NSX_TIMER_TEMPCO] == NULL);
    assert(allocations == releases);
}
static void setup_success(void) {
    step = fail_step = 0; cleanup = false;
    assert(nsx_power_tempco_enable() == 0);
    assert(allocated && powered && enabled && running && irq_enabled && s_nsx_power_tempco.initialized);
}
int main(int argc,char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1],"unsupported") || !strcmp(argv[1],"trim-read-failure")) {
        bad_trim = !strcmp(argv[1],"unsupported"); trim_read_failure = !bad_trim;
        assert(nsx_power_tempco_enable() == (bad_trim ? (uint32_t)NSX_STATUS_FAILURE : FAIL)); assert(operations == 0); clean(); return 0;
    }
    setup_success(); unsigned setup_steps = step;
    if (!strcmp(argv[1],"success")) {
        unsigned before = operations; assert(nsx_power_tempco_enable() == 0); assert(operations == before);
        nsx_power_tempco_prepare_for_sleep(); assert(!powered);
    }
    cleanup = true; assert(nsx_power_tempco_disable() == 0); clean();
    if (!strcmp(argv[1],"init-failures")) {
        for (unsigned i=1;i<=setup_steps;++i) {
            step=0; fail_step=i; cleanup=false;
            assert(nsx_power_tempco_enable() == FAIL); clean();
            setup_success(); cleanup=true; assert(nsx_power_tempco_disable() == 0); clean();
        }
    }
    if (!strcmp(argv[1],"init-cleanup-failure")) {
        step = 0; fail_step = setup_steps; cleanup = false;
        cleanup_failure = "timer-stop";
        assert(nsx_power_tempco_enable() == FAIL);
        assert(!s_nsx_power_tempco.initialized && allocated);
        assert(s_nsx_power_tempco.cleanup_status == CLEAN_FAIL);
        unsigned before = allocations;
        assert(nsx_power_tempco_enable() == CLEAN_FAIL); assert(allocations == before);
        cleanup_failure = NULL; fail_step = 0;
        assert(nsx_power_tempco_enable() == 0);
        assert(nsx_power_tempco_disable() == 0); clean();
    }
    if (!strcmp(argv[1],"callback-failures")) {
        for (unsigned i=1;i<=2;++i) {
            setup_success(); nsx_power_tempco_prepare_for_sleep();
            step=0; fail_step=i; cleanup=false;
            nsx_power_tempco_callback(&s_nsx_power_tempco_timer);
            assert(s_nsx_power_tempco.last_handler_status == FAIL);
            assert(nsx_power_tempco_disable() == 0); clean();
        }
    }
    if (!strcmp(argv[1],"cleanup-failures")) {
        const char *failures[] = {"unregister","timer-irq-disable","timer-stop","timer-irq-clear","pending-clear","disable","power-off"};
        for (unsigned i=0;i<sizeof failures/sizeof failures[0];++i) {
            setup_success(); cleanup=true; cleanup_failure=failures[i];
            assert(nsx_power_tempco_disable() == CLEAN_FAIL);
            assert(!s_nsx_power_tempco.initialized && allocated);
            unsigned before=allocations;
            assert(nsx_power_tempco_enable() == CLEAN_FAIL); assert(allocations == before);
            cleanup_failure=NULL; assert(nsx_power_tempco_disable() == 0); clean();
        }
        setup_success(); cleanup=true; cleanup_failure="deallocate"; deinit_poison=true;
        assert(nsx_power_tempco_disable() == CLEAN_FAIL);
        unsigned before=operations; cleanup_failure=NULL;
        assert(nsx_power_tempco_enable() == CLEAN_FAIL); assert(operations == before);
        return 0;
    }
    if (!strcmp(argv[1],"timer-owner")) {
        nsx_timer_config_t other=s_nsx_power_tempco_timer;
        assert(nsx_timer_init(&other) == 0);
        unsigned before=operations;
        assert(nsx_timer_init(&s_nsx_power_tempco_timer) == NSX_STATUS_INVALID_CONFIG);
        assert(nsx_timer_deinit(&s_nsx_power_tempco_timer) == NSX_STATUS_INVALID_CONFIG);
        assert(before == operations);
        assert(nsx_power_tempco_enable() == NSX_STATUS_INVALID_CONFIG);
        assert(nsx_timer_config[NSX_TIMER_TEMPCO] == &other && running);
        assert(nsx_timer_deinit(&other) == 0);
        assert(nsx_power_tempco_enable() == 0);
        assert(nsx_power_tempco_disable() == 0);
        assert(nsx_timer_deinit(&other) == 0); clean();
    }
    return 0;
}
