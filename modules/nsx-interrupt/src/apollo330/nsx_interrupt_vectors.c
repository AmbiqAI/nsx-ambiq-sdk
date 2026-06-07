#include "nsx_interrupt.h"

//
// Apollo330P GPIO interrupt vector glue.
//
// Apollo330P shares the same GPIO bank ISR symbol names and IRQn layout as the
// rest of this SDK's Apollo5 family, so the dispatch glue is identical. These
// strong definitions override the weak `am_*_isr` aliases declared in
// nsx-core's startup_gcc.c and forward to the generic nsx-interrupt dispatcher.
//
// Referenced by nsx_irq_vectors_anchor in nsx_interrupt.c to force this
// translation unit to be linked out of the static archive.
//
const uint32_t nsx_irq_vectors_present = 1;

#define NSX_IRQ_VECTOR(name, irqn)                                             \
    void name(void) { nsx_irq_dispatch(irqn); }

// GPIO bank 0 (pads 0..255)
NSX_IRQ_VECTOR(am_gpio0_001f_isr, GPIO0_001F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_203f_isr, GPIO0_203F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_405f_isr, GPIO0_405F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_607f_isr, GPIO0_607F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_809f_isr, GPIO0_809F_IRQn)
NSX_IRQ_VECTOR(am_gpio0_a0bf_isr, GPIO0_A0BF_IRQn)
NSX_IRQ_VECTOR(am_gpio0_c0df_isr, GPIO0_C0DF_IRQn)
NSX_IRQ_VECTOR(am_gpio0_e0ff_isr, GPIO0_E0FF_IRQn)

// GPIO bank 1 (pads 256..)
NSX_IRQ_VECTOR(am_gpio1_001f_isr, GPIO1_001F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_203f_isr, GPIO1_203F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_405f_isr, GPIO1_405F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_607f_isr, GPIO1_607F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_809f_isr, GPIO1_809F_IRQn)
NSX_IRQ_VECTOR(am_gpio1_a0bf_isr, GPIO1_A0BF_IRQn)
NSX_IRQ_VECTOR(am_gpio1_c0df_isr, GPIO1_C0DF_IRQn)

// UART lines
NSX_IRQ_VECTOR(am_uart_isr, UART0_IRQn)
NSX_IRQ_VECTOR(am_uart1_isr, UART1_IRQn)

// MSPI lines (Apollo330P exposes MSPI0..2).
NSX_IRQ_VECTOR(am_mspi0_isr, MSPI0_IRQn)
NSX_IRQ_VECTOR(am_mspi1_isr, MSPI1_IRQn)
NSX_IRQ_VECTOR(am_mspi2_isr, MSPI2_IRQn)

// Timer lines
NSX_IRQ_VECTOR(am_timer01_isr, TIMER1_IRQn)
NSX_IRQ_VECTOR(am_timer02_isr, TIMER2_IRQn)
NSX_IRQ_VECTOR(am_timer03_isr, TIMER3_IRQn)
NSX_IRQ_VECTOR(am_timer04_isr, TIMER4_IRQn)
