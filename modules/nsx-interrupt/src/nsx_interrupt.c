#include "nsx_interrupt.h"

const nsx_core_api_t nsx_interrupt_V0_0_1 = {
    .apiId = NSX_INTERRUPT_API_ID, .version = NSX_INTERRUPT_V0_0_1};

const nsx_core_api_t nsx_interrupt_oldest_supported_version = {
    .apiId = NSX_INTERRUPT_API_ID,
    .version = NSX_INTERRUPT_OLDEST_SUPPORTED_VERSION};

const nsx_core_api_t nsx_interrupt_current_version = {
    .apiId = NSX_INTERRUPT_API_ID,
    .version = NSX_INTERRUPT_CURRENT_VERSION};

//
// Sized to the SoC's full IRQ space. MAX_IRQn is provided by the device CMSIS
// header (apollo510_generic.h / apollo330P.h) and is "one past" the last valid
// IRQ line, so it doubles as the table length.
//
#define NSX_IRQ_TABLE_SIZE ((uint32_t)MAX_IRQn)

typedef struct {
    nsx_irq_handler_t handler;
    void *ctx;
} nsx_irq_entry_t;

static nsx_irq_entry_t g_nsx_irq_table[NSX_IRQ_TABLE_SIZE];

//
// Anchor that forces the family-specific vector translation unit (which holds
// the strong am_gpio*_isr definitions) to be pulled out of the static archive.
// Without an undefined reference into that object, the linker keeps the weak
// default aliases from startup_gcc.c and our dispatchers are never installed.
//
extern const uint32_t nsx_irq_vectors_present;
const uint32_t *const nsx_irq_vectors_anchor = &nsx_irq_vectors_present;

static inline bool
nsx_irq_in_range(IRQn_Type irqn)
{
    return (int32_t)irqn >= 0 && (uint32_t)irqn < NSX_IRQ_TABLE_SIZE;
}

uint32_t
nsx_irq_register(const nsx_irq_config_t *cfg)
{
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL || cfg->handler == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(cfg->api, &nsx_interrupt_oldest_supported_version,
                          &nsx_interrupt_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (!nsx_irq_in_range(cfg->irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    g_nsx_irq_table[cfg->irqn].handler = cfg->handler;
    g_nsx_irq_table[cfg->irqn].ctx = cfg->ctx;

    NVIC_SetPriority(cfg->irqn, cfg->priority);
    NVIC_ClearPendingIRQ(cfg->irqn);
    if (cfg->enable) {
        NVIC_EnableIRQ(cfg->irqn);
    }

    return NSX_STATUS_SUCCESS;
}

uint32_t
nsx_irq_unregister(IRQn_Type irqn)
{
    if (!nsx_irq_in_range(irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    NVIC_DisableIRQ(irqn);
    g_nsx_irq_table[irqn].handler = NULL;
    g_nsx_irq_table[irqn].ctx = NULL;
    return NSX_STATUS_SUCCESS;
}

uint32_t
nsx_irq_enable(IRQn_Type irqn)
{
    if (!nsx_irq_in_range(irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    NVIC_EnableIRQ(irqn);
    return NSX_STATUS_SUCCESS;
}

uint32_t
nsx_irq_disable(IRQn_Type irqn)
{
    if (!nsx_irq_in_range(irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    NVIC_DisableIRQ(irqn);
    return NSX_STATUS_SUCCESS;
}

uint32_t
nsx_irq_clear_pending(IRQn_Type irqn)
{
    if (!nsx_irq_in_range(irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    NVIC_ClearPendingIRQ(irqn);
    return NSX_STATUS_SUCCESS;
}

uint32_t
nsx_irq_set_priority(IRQn_Type irqn, uint32_t priority)
{
    if (!nsx_irq_in_range(irqn)) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    NVIC_SetPriority(irqn, priority);
    return NSX_STATUS_SUCCESS;
}

void
nsx_irq_dispatch(IRQn_Type irqn)
{
    if (!nsx_irq_in_range(irqn)) {
        return;
    }

    nsx_irq_handler_t handler = g_nsx_irq_table[irqn].handler;
    if (handler != NULL) {
        handler(g_nsx_irq_table[irqn].ctx);
    }
}
