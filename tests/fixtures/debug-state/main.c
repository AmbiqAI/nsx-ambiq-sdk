#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "nsx_system.h"
#include "am_bsp.h"
#include "am_util_stdio.h"

static unsigned enable_calls, disable_calls, print_calls, fail_enable, fail_disable, bsp_calls;
static nsx_debug_transport_e route;
const nsx_core_api_t nsx_core_V1_0_0 = {0};
uint32_t nsx_core_init(nsx_core_config_t *cfg) {(void)cfg; return 0;}
uint32_t nsx_cache_enable(void) {return 0;}
uint32_t nsx_platform_hw_init(void) {assert(route == NSX_DEBUG_NONE); ++bsp_calls; return 0;}
uint32_t nsx_platform_minimal_hw_init(void) {return 0;}
uint32_t nsx_platform_set_perf_mode(nsx_perf_mode_e mode) {(void)mode; return 0;}
uint32_t nsx_platform_spot_mgr_profile(void) {return 0;}
uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg) {
    ++enable_calls;
    route=cfg->transport; /* Even a failing HAL can alter stdio/hardware. */
    return fail_enable;
}
uint32_t nsx_platform_debug_disable(nsx_debug_transport_e transport) {
    assert(route == transport);
    ++disable_calls; route=NSX_DEBUG_NONE; return fail_disable;
}
void am_util_stdio_vprintf(const char *fmt, va_list args) {
    (void)fmt; (void)args; assert(route != NSX_DEBUG_NONE); ++print_calls;
}
void am_util_delay_us(unsigned us) {(void)us;}
uint32_t am_hal_interrupt_master_disable(void) {return 0;}
void am_hal_interrupt_master_enable(void) {}
void am_hal_interrupt_master_set(uint32_t mask) {(void)mask;}

static void output(unsigned expected) {
    nsx_low_power_printf("message %d", 1);
    assert(print_calls == expected);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    nsx_debug_config_t itm={.transport=NSX_DEBUG_ITM}, uart={.transport=NSX_DEBUG_UART};
    nsx_system_config_t system={.debug={.transport=NSX_DEBUG_ITM}};
    if (!strcmp(argv[1], "system")) {
        output(0);
        assert(nsx_system_init(&system) == 0);
        output(1);
        nsx_itm_printf_enable(); nsx_debug_printf_enable();
        assert(enable_calls == 1); output(2);
    } else if (!strcmp(argv[1], "resume")) {
        nsx_debug_printf_enable(); output(1);
        assert(route == NSX_DEBUG_ITM);
        nsx_uart_printf_disable(); output(2); assert(disable_calls == 0);
        nsx_debug_printf_disable(); output(2);
        nsx_debug_printf_disable(); assert(disable_calls == 1);
        nsx_debug_printf_enable(); output(3); assert(enable_calls == 2);
    } else if (!strcmp(argv[1], "switch")) {
        assert(nsx_debug_init(&itm) == 0); output(1);
        nsx_uart_printf_enable(); assert(disable_calls == 1 && route == NSX_DEBUG_UART); output(2);
        nsx_itm_printf_disable(); output(3); assert(disable_calls == 1);
        nsx_uart_printf_disable(); output(3);
        nsx_debug_printf_enable(); assert(route == NSX_DEBUG_UART); output(4);
        assert(nsx_debug_init(&itm) == 0); assert(route == NSX_DEBUG_ITM); output(5);
    } else if (!strcmp(argv[1], "noop")) {
        assert(nsx_debug_init(&itm) == 0);
        nsx_debug_config_t cfg={0}; assert(nsx_debug_init(NULL) == 0 && nsx_debug_init(&cfg) == 0);
        cfg.transport=(nsx_debug_transport_e)99;
        assert(nsx_debug_init(&cfg) == NSX_STATUS_INVALID_CONFIG);
        output(1); assert(enable_calls == 1 && disable_calls == 0);
    } else if (!strcmp(argv[1], "setup-failure") || !strcmp(argv[1], "switch-failure")) {
        if (!strcmp(argv[1], "switch-failure")) assert(nsx_debug_init(&itm) == 0);
        fail_enable=0x61;
        assert(nsx_debug_init(&uart) == 0x61); output(0);
        unsigned calls=enable_calls;
        fail_enable=0;
        assert(nsx_debug_init(&itm) == 0x61);
        nsx_uart_printf_enable(); nsx_debug_printf_enable(); nsx_debug_printf_disable();
        assert(enable_calls == calls); output(0);
    } else if (!strcmp(argv[1], "disable-failure")) {
        assert(nsx_debug_init(&itm) == 0); output(1);
        fail_disable=0x62;
        assert(nsx_debug_init(&uart) == 0x62);
        assert(enable_calls == 1 && disable_calls == 1); output(1);
        fail_disable=0;
        nsx_debug_printf_enable(); assert(enable_calls == 1); output(1);
    } else if (!strcmp(argv[1], "hardware-reset")) {
        assert(nsx_debug_init(&itm) == 0); output(1);
        assert(nsx_hw_init() == 0 && bsp_calls == 1 && disable_calls == 1); output(1);
        assert(nsx_debug_init(&itm) == 0 && enable_calls == 2); output(2);
    } else if (!strcmp(argv[1], "system-reapply")) {
        assert(nsx_debug_init(&itm) == 0);
        assert(nsx_system_init(&system) == 0);
        assert(enable_calls == 2 && disable_calls == 1); output(1);
    } else if (!strcmp(argv[1], "hardware-release-failure")) {
        assert(nsx_debug_init(&uart) == 0);
        fail_disable=0x63;
        assert(nsx_hw_init() == 0x63 && bsp_calls == 0 && disable_calls == 1);
        output(0);
    } else return 2;
    puts("PASS");
}
