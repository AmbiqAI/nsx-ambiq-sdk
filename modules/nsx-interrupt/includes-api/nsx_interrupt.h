#ifndef NSX_INTERRUPT_H
#define NSX_INTERRUPT_H

// Ambiq/CMSIS headers include overloaded MVE intrinsics in C++ mode.
// Keep them out of any caller-provided extern "C" block.
#ifdef __cplusplus
extern "C++" {
#endif
#include "am_mcu_apollo.h"
#ifdef __cplusplus
}
#endif

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_INTERRUPT_V0_0_1 { .major = 0, .minor = 0, .revision = 1 }
#define NSX_INTERRUPT_OLDEST_SUPPORTED_VERSION NSX_INTERRUPT_V0_0_1
#define NSX_INTERRUPT_CURRENT_VERSION NSX_INTERRUPT_V0_0_1
#define NSX_INTERRUPT_API_ID 0xCA0011

extern const nsx_core_api_t nsx_interrupt_V0_0_1;
extern const nsx_core_api_t nsx_interrupt_oldest_supported_version;
extern const nsx_core_api_t nsx_interrupt_current_version;

//
// nsx-interrupt is the per-SDK interrupt-dispatch layer. It owns the raw
// startup vector symbols (e.g. am_gpio0_001f_isr) and routes each NVIC IRQ
// line to a single registered handler. Higher-level wrappers (nsx-gpio,
// nsx-uart, nsx-timer) register a handler per IRQ line and layer their own
// semantics (pin/bank dispatch, RX/TX callbacks) on top.
//
// This module is scoped to the SoCs of this SDK (the Apollo5 family:
// Apollo510 / Apollo510B / Apollo510L / Apollo330P). Each SDK ships its own
// nsx-interrupt covering only that SDK's parts.
//

//! Handler invoked from interrupt context for a registered IRQ line.
typedef void (*nsx_irq_handler_t)(void *ctx);

typedef struct {
    const nsx_core_api_t *api; //!< Must point to nsx_interrupt_current_version.
    IRQn_Type irqn;           //!< NVIC IRQ line to register.
    nsx_irq_handler_t handler;//!< Dispatched when the line fires.
    void *ctx;                //!< Opaque context passed to the handler.
    uint32_t priority;        //!< NVIC priority (lower value == higher prio).
    bool enable;              //!< Enable the NVIC line immediately on register.
} nsx_irq_config_t;

//! Register a handler for a single IRQ line. Replaces any prior handler.
uint32_t nsx_irq_register(const nsx_irq_config_t *cfg);

//! Remove the handler for an IRQ line and disable it in the NVIC.
uint32_t nsx_irq_unregister(IRQn_Type irqn);

//! Enable / disable an IRQ line in the NVIC.
uint32_t nsx_irq_enable(IRQn_Type irqn);
uint32_t nsx_irq_disable(IRQn_Type irqn);

//! Clear a pending IRQ in the NVIC.
uint32_t nsx_irq_clear_pending(IRQn_Type irqn);

//! Set the NVIC priority for an IRQ line.
uint32_t nsx_irq_set_priority(IRQn_Type irqn, uint32_t priority);

//! Invoke the registered handler for an IRQ line. Called from vector glue
//! (interrupt context). No-op if no handler is registered.
void nsx_irq_dispatch(IRQn_Type irqn);

#ifdef __cplusplus
}
#endif

#endif // NSX_INTERRUPT_H
