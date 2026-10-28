#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "nsx_timer.h"
#include "nsx_interrupt.h"

extern nsx_timer_config_t *nsx_timer_config[];
const nsx_core_api_t nsx_interrupt_current_version = {0};
static am_hal_timer_config_t recorded;
static unsigned operations, clears, acknowledgements, callbacks;
static uint32_t read_ticks, default_status;
static nsx_irq_handler_t handler;
static void *context;
static nsx_timer_config_t cfg;
void nsx_core_fail_loop(void) { abort(); }
void nsx_printf(const char *format, ...) { (void)format; }
uint32_t nsx_core_check_api(const nsx_core_api_t *a,const nsx_core_api_t *b,const nsx_core_api_t *c) {
    (void)a; (void)b; (void)c; return 0;
}
uint32_t am_hal_interrupt_master_disable(void) { return 0; }
void am_hal_interrupt_master_set(uint32_t mask) { (void)mask; }
uint32_t am_hal_timer_default_config_set(am_hal_timer_config_t *c) {
    ++operations;
    *c=(am_hal_timer_config_t){.eInputClock=99,.ui32Compare0=UINT32_MAX,.ui32Compare1=UINT32_MAX};
    return default_status;
}
uint32_t am_hal_timer_config(uint32_t n, am_hal_timer_config_t *c) {
    assert(n == (uint32_t)cfg.timer); ++operations; recorded=*c; return 0;
}
uint32_t am_hal_timer_clear(uint32_t n) { assert(n==(uint32_t)cfg.timer); ++operations; ++clears; return 0; }
uint32_t am_hal_timer_start(uint32_t n) { assert(n==(uint32_t)cfg.timer); ++operations; return 0; }
uint32_t am_hal_timer_stop(uint32_t n) { (void)n; ++operations; return 0; }
uint32_t am_hal_timer_interrupt_clear(uint32_t m) {
    assert(m==AM_HAL_TIMER_MASK(cfg.timer,AM_HAL_TIMER_COMPARE1)); ++operations; ++acknowledgements; return 0;
}
uint32_t am_hal_timer_interrupt_enable(uint32_t m) { (void)m; ++operations; return 0; }
uint32_t am_hal_timer_interrupt_disable(uint32_t m) { (void)m; ++operations; return 0; }
uint32_t am_hal_timer_read(uint32_t n) { assert(n==(uint32_t)cfg.timer); return read_ticks; }
uint32_t nsx_irq_register(const nsx_irq_config_t *c) { handler=c->handler; context=c->ctx; ++operations; return 0; }
uint32_t nsx_irq_unregister(IRQn_Type n) { (void)n; ++operations; return 0; }
uint32_t nsx_irq_clear_pending(IRQn_Type n) { (void)n; ++operations; return 0; }
static void callback(nsx_timer_config_t *c) {
    assert(c==&cfg);
    assert(clears==callbacks+2 && acknowledgements==callbacks+2);
    ++callbacks;
}
int main(int argc,char **argv) {
    assert(argc==4);
    cfg=(nsx_timer_config_t){.api=&nsx_timer_current_version,.timer=(nsx_timers_e)atoi(argv[2]),
        .enableInterrupt=true,.periodInMicroseconds=(uint32_t)strtoull(argv[3],NULL,10),.callback=callback};
    if (!strcmp(argv[1],"counter")) cfg.enableInterrupt=false;
    if (!strcmp(argv[1],"default-failure")) default_status=0x51;
    uint32_t status=nsx_timer_init(&cfg);
    if (default_status) { assert(status==default_status && operations==1); return 0; }
    if (cfg.enableInterrupt && (!cfg.periodInMicroseconds || cfg.periodInMicroseconds>715827882u)) {
        assert(status==NSX_STATUS_INVALID_CONFIG && operations==0);
        assert(nsx_timer_config[cfg.timer]==NULL); return 0;
    }
    assert(status==0 && recorded.eInputClock==AM_HAL_TIMER_CLOCK_HFRC_DIV16);
    if (cfg.enableInterrupt) {
        /* Independent units: microseconds * 6,000,000 ticks/second. */
        assert(recorded.ui32Compare1==(uint64_t)cfg.periodInMicroseconds*6000000/1000000);
        assert(recorded.ui32Compare1<recorded.ui32Compare0);
        assert(recorded.eFunction==AM_HAL_TIMER_FN_UPCOUNT && handler);
        handler(context); handler(context); assert(callbacks==2);
        unsigned before=operations;
        nsx_timer_config_t invalid=cfg; invalid.periodInMicroseconds=UINT32_MAX;
        assert(nsx_timer_init(&invalid)==NSX_STATUS_INVALID_CONFIG);
        assert(operations==before && nsx_timer_config[cfg.timer]==&cfg);
        cfg.periodInMicroseconds=0;
        assert(nsx_timer_init(&cfg)==NSX_STATUS_INVALID_CONFIG && operations==before);
    } else { assert(!handler); }
    const uint32_t ticks[]={0,1,5,6,7,UINT32_MAX};
    const uint32_t micros[]={0,0,0,1,1,715827882};
    for (unsigned i=0;i<sizeof ticks/sizeof ticks[0];++i) {
        read_ticks=ticks[i]; assert(nsx_timer_us_read(&cfg)==micros[i]);
    }
    return 0;
}
