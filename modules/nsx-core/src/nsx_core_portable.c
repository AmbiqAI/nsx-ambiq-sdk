#include "nsx_core.h"
#include "nsx_system.h"
#include "nsx_debug_internal.h"

#include <stdarg.h>

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "am_util_stdio.h"

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

    if (!nsx_debug_output_enabled()) {
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
    nsx_debug_resume();
}

void nsx_debug_printf_disable(void)
{
    nsx_debug_suspend();
}

void nsx_itm_printf_enable(void)
{
    nsx_debug_config_t cfg = { .transport = NSX_DEBUG_ITM };
    (void)nsx_debug_init(&cfg);
}

void nsx_itm_printf_disable(void)
{
    nsx_debug_disable(NSX_DEBUG_ITM);
}

void nsx_uart_printf_enable(void)
{
    nsx_debug_config_t cfg = { .transport = NSX_DEBUG_UART };
    (void)nsx_debug_init(&cfg);
}

void nsx_uart_printf_disable(void)
{
    nsx_debug_disable(NSX_DEBUG_UART);
}
