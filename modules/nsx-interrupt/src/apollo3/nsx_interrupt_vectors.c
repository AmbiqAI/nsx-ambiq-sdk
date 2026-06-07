#include "nsx_interrupt.h"

// Apollo3/Apollo3p GPIO interrupt vector glue.
// Both parts expose a single GPIO_IRQn startup vector, so nsx-interrupt owns
// am_gpio_isr and dispatches that one line through its registry.
const uint32_t nsx_irq_vectors_present = 1;

void
am_gpio_isr(void)
{
    nsx_irq_dispatch(GPIO_IRQn);
}

void
am_uart_isr(void)
{
    nsx_irq_dispatch(UART0_IRQn);
}

void
am_uart1_isr(void)
{
    nsx_irq_dispatch(UART1_IRQn);
}

void
am_mspi0_isr(void)
{
    nsx_irq_dispatch(MSPI0_IRQn);
}

#if defined(AM_PART_APOLLO3P)
void
am_mspi1_isr(void)
{
    nsx_irq_dispatch(MSPI1_IRQn);
}

void
am_mspi2_isr(void)
{
    nsx_irq_dispatch(MSPI2_IRQn);
}
#endif

void
am_stimer_cmpr0_isr(void)
{
    nsx_irq_dispatch(STIMER_CMPR0_IRQn);
}

void
am_stimer_cmpr1_isr(void)
{
    nsx_irq_dispatch(STIMER_CMPR1_IRQn);
}

void
am_stimer_cmpr2_isr(void)
{
    nsx_irq_dispatch(STIMER_CMPR2_IRQn);
}

void
am_stimer_cmpr3_isr(void)
{
    nsx_irq_dispatch(STIMER_CMPR3_IRQn);
}
