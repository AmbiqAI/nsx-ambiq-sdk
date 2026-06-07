#include "nsx_core.h"
#include "nsx_system.h"

#include <stdarg.h>

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "am_util_stdio.h"

/* Debug-transport state is private to the runtime layer. It is never exposed
 * in the public API and no other module may reach into it — transports are
 * controlled exclusively through the nsx_*_printf_enable()/disable() helpers. */
static struct {
    bool itm_enabled;
    bool uart_enabled;
} s_debug_state;

void nsx_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    am_util_stdio_vprintf(fmt, args);
    va_end(args);
}

void nsx_low_power_printf(const char *fmt, ...)
{
    va_list args;

    if (!s_debug_state.uart_enabled && !s_debug_state.itm_enabled) {
        return;
    }

    va_start(args, fmt);
    am_util_stdio_vprintf(fmt, args);
    va_end(args);
}

void nsx_delay_us(uint32_t usec)
{
    am_util_delay_us(usec);
}

void nsx_interrupt_master_enable(void)
{
    am_hal_interrupt_master_enable();
}

void nsx_interrupt_master_disable(void)
{
    am_hal_interrupt_master_disable();
}

void nsx_debug_printf_enable(void)
{
    am_bsp_debug_printf_enable();
}

void nsx_debug_printf_disable(void)
{
    am_bsp_debug_printf_disable();
}

void nsx_itm_printf_enable(void)
{
    nsx_debug_config_t cfg = { .transport = NSX_DEBUG_ITM };
    if (nsx_debug_init(&cfg) == 0) {
        s_debug_state.itm_enabled = true;
    }
}

void nsx_itm_printf_disable(void)
{
    am_bsp_itm_printf_disable();
    s_debug_state.itm_enabled = false;
}

void nsx_uart_printf_enable(void)
{
    nsx_debug_config_t cfg = { .transport = NSX_DEBUG_UART };
    if (nsx_debug_init(&cfg) == 0) {
        s_debug_state.uart_enabled = true;
    }
}

void nsx_uart_printf_disable(void)
{
    am_bsp_uart_printf_disable();
    s_debug_state.uart_enabled = false;
}
