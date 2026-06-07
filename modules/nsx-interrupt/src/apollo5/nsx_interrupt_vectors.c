#include "nsx_interrupt.h"

//
// Apollo5-family GPIO interrupt vector glue.
//
// These strong definitions override the weak `am_*_isr` aliases declared in
// nsx-core's startup_gcc.c. Each vector forwards to the generic nsx-interrupt
// dispatcher, which invokes whatever handler the upper layer (nsx-gpio)
// registered for that bank's IRQ line.
//
// Apollo510 exposes 224 GPIO pads, so only the GPIO0 banks (IRQ 56..63) are
// populated on that part; the GPIO1 banks are present for other parts in this
// family that expose pads above 255. Defining all of them keeps this file
// SoC-agnostic within the family — unused banks simply never fire.
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

// UART lines. UART0/UART1 exist across the family; UART2/UART3 are only present
// on the full Apollo510 part (not Apollo510L), so they are gated on the part
// macro.
NSX_IRQ_VECTOR(am_uart_isr, UART0_IRQn)
NSX_IRQ_VECTOR(am_uart1_isr, UART1_IRQn)
#if defined(AM_PART_APOLLO510)
NSX_IRQ_VECTOR(am_uart2_isr, UART2_IRQn)
NSX_IRQ_VECTOR(am_uart3_isr, UART3_IRQn)
#endif

// MSPI lines. MSPI0..2 exist across the family; MSPI3 is only present on the
// full Apollo510 part (not Apollo510L), so its IRQn enumerator is gated on the
// part macro.
NSX_IRQ_VECTOR(am_mspi0_isr, MSPI0_IRQn)
NSX_IRQ_VECTOR(am_mspi1_isr, MSPI1_IRQn)
NSX_IRQ_VECTOR(am_mspi2_isr, MSPI2_IRQn)
#if defined(AM_PART_APOLLO510)
NSX_IRQ_VECTOR(am_mspi3_isr, MSPI3_IRQn)
#endif

// Timer lines
NSX_IRQ_VECTOR(am_timer01_isr, TIMER1_IRQn)
NSX_IRQ_VECTOR(am_timer02_isr, TIMER2_IRQn)
NSX_IRQ_VECTOR(am_timer03_isr, TIMER3_IRQn)
NSX_IRQ_VECTOR(am_timer04_isr, TIMER4_IRQn)
