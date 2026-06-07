#include "nsx_interrupt.h"

// Apollo4 GPIO interrupt vector glue.
// GPIO0_* and GPIO1_* are the two Apollo4 interrupt channels; each channel has
// four 32-pin banks covering pins 0..127.
const uint32_t nsx_irq_vectors_present = 1;

#define NSX_IRQ_VECTOR(name, irqn) \
    void name(void) { nsx_irq_dispatch(irqn); }

NSX_IRQ_VECTOR(am_gpio0_001f_isr, GPIO0_001F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_203f_isr, GPIO0_203F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_405f_isr, GPIO0_405F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_607f_isr, GPIO0_607F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_001f_isr, GPIO1_001F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_203f_isr, GPIO1_203F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_405f_isr, GPIO1_405F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_607f_isr, GPIO1_607F_IRQn)

NSX_IRQ_VECTOR(am_uart_isr, UART0_IRQn)
NSX_IRQ_VECTOR(am_uart1_isr, UART1_IRQn)
NSX_IRQ_VECTOR(am_uart2_isr, UART2_IRQn)
NSX_IRQ_VECTOR(am_uart3_isr, UART3_IRQn)

NSX_IRQ_VECTOR(am_mspi0_isr, MSPI0_IRQn)
NSX_IRQ_VECTOR(am_mspi1_isr, MSPI1_IRQn)
NSX_IRQ_VECTOR(am_mspi2_isr, MSPI2_IRQn)

NSX_IRQ_VECTOR(am_timer01_isr, TIMER1_IRQn)
NSX_IRQ_VECTOR(am_timer02_isr, TIMER2_IRQn)
NSX_IRQ_VECTOR(am_timer03_isr, TIMER3_IRQn)
NSX_IRQ_VECTOR(am_timer04_isr, TIMER4_IRQn)
