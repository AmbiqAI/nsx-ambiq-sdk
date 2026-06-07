#include "nsx_gpio.h"
#include "nsx_interrupt.h"

const nsx_core_api_t nsx_gpio_V0_0_1 = {.apiId = NSX_GPIO_API_ID, .version = NSX_GPIO_V0_0_1};

const nsx_core_api_t nsx_gpio_oldest_supported_version = {
    .apiId = NSX_GPIO_API_ID, .version = NSX_GPIO_OLDEST_SUPPORTED_VERSION};

const nsx_core_api_t nsx_gpio_current_version = {
    .apiId = NSX_GPIO_API_ID, .version = NSX_GPIO_CURRENT_VERSION};

static const am_hal_gpio_pincfg_t *
nsx_gpio_mode_to_pincfg(nsx_gpio_mode_t mode)
{
    switch (mode) {
    case NSX_GPIO_MODE_DISABLED:
        return &am_hal_gpio_pincfg_disabled;
    case NSX_GPIO_MODE_INPUT:
        return &am_hal_gpio_pincfg_input;
    case NSX_GPIO_MODE_OUTPUT:
        return &am_hal_gpio_pincfg_output;
    case NSX_GPIO_MODE_OUTPUT_WITH_READ:
        return &am_hal_gpio_pincfg_output_with_read;
    default:
        return NULL;
    }
}

// ---------------------------------------------------------------------------
// Pin interrupt dispatch
//
// nsx-gpio layers pin-level semantics on top of nsx-interrupt's IRQ-line
// dispatch. The AmbiqSuite HAL already maintains a per-pin handler table
// (am_hal_gpio_interrupt_register/_service), so nsx-gpio:
//   1. registers a small trampoline per pin with the HAL to surface the
//      (pin, ctx) signature, and
//   2. registers ONE bank handler per GPIO IRQ line with nsx-interrupt that
//      drains the bank status and fans out via the HAL service routine.
// ---------------------------------------------------------------------------

typedef struct {
    nsx_gpio_irq_cb_t cb;
    void *ctx;
    uint32_t pin;
} nsx_gpio_pin_entry_t;

static nsx_gpio_pin_entry_t g_nsx_gpio_pins[AM_HAL_GPIO_MAX_PADS];

static IRQn_Type
nsx_gpio_pin_to_irqn(uint32_t pin)
{
    if (pin < 256) {
        return (IRQn_Type)((uint32_t)GPIO0_001F_IRQn + (pin >> 5));
    }
    return (IRQn_Type)((uint32_t)GPIO1_001F_IRQn + ((pin - 256) >> 5));
}

// HAL per-pin trampoline: pArg points at the owning pin entry.
static void
nsx_gpio_pin_trampoline(void *pArg)
{
    nsx_gpio_pin_entry_t *entry = (nsx_gpio_pin_entry_t *)pArg;
    if (entry != NULL && entry->cb != NULL) {
        entry->cb(entry->pin, entry->ctx);
    }
}

// Bank handler registered with nsx-interrupt; ctx carries the bank IRQn.
static void
nsx_gpio_bank_handler(void *ctx)
{
    uint32_t irqn = (uint32_t)(uintptr_t)ctx;
    uint32_t status = 0;

    am_hal_gpio_interrupt_irq_status_get(irqn, true, &status);
    am_hal_gpio_interrupt_irq_clear(irqn, status);
    am_hal_gpio_interrupt_service(irqn, status);
}

static am_hal_gpio_intdir_e
nsx_gpio_trigger_to_intdir(nsx_gpio_trigger_t trigger)
{
    switch (trigger) {
    case NSX_GPIO_TRIGGER_RISING:
        return AM_HAL_GPIO_PIN_INTDIR_LO2HI;
    case NSX_GPIO_TRIGGER_FALLING:
        return AM_HAL_GPIO_PIN_INTDIR_HI2LO;
    case NSX_GPIO_TRIGGER_BOTH:
        return AM_HAL_GPIO_PIN_INTDIR_BOTH;
    default:
        return AM_HAL_GPIO_PIN_INTDIR_NONE;
    }
}

static uint32_t
nsx_gpio_init_interrupt(const nsx_gpio_config_t *cfg)
{
    if (cfg->irq_cb == NULL || cfg->pin >= AM_HAL_GPIO_MAX_PADS) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    am_hal_gpio_pincfg_t pincfg = am_hal_gpio_pincfg_input;
    pincfg.GP.cfg_b.eGPInput = AM_HAL_GPIO_PIN_INPUT_ENABLE;
    pincfg.GP.cfg_b.eIntDir = nsx_gpio_trigger_to_intdir(cfg->trigger);

    uint32_t status = am_hal_gpio_pinconfig(cfg->pin, pincfg);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    g_nsx_gpio_pins[cfg->pin].cb = cfg->irq_cb;
    g_nsx_gpio_pins[cfg->pin].ctx = cfg->irq_ctx;
    g_nsx_gpio_pins[cfg->pin].pin = cfg->pin;

    am_hal_gpio_interrupt_register(AM_HAL_GPIO_INT_CHANNEL_0, cfg->pin,
                                   nsx_gpio_pin_trampoline,
                                   &g_nsx_gpio_pins[cfg->pin]);

    IRQn_Type irqn = nsx_gpio_pin_to_irqn(cfg->pin);
    nsx_irq_config_t irq_cfg = {
        .api = &nsx_interrupt_current_version,
        .irqn = irqn,
        .handler = nsx_gpio_bank_handler,
        .ctx = (void *)(uintptr_t)irqn,
        .priority = AM_IRQ_PRIORITY_DEFAULT,
        .enable = true,
    };
    status = nsx_irq_register(&irq_cfg);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }

    return nsx_gpio_irq_enable(cfg->pin);
}

uint32_t
nsx_gpio_init(const nsx_gpio_config_t *cfg)
{
#ifndef NSX_DISABLE_API_VALIDATION
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(
            cfg->api, &nsx_gpio_oldest_supported_version, &nsx_gpio_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    if (cfg->trigger != NSX_GPIO_TRIGGER_NONE) {
        return nsx_gpio_init_interrupt(cfg);
    }

    const am_hal_gpio_pincfg_t *pincfg = nsx_gpio_mode_to_pincfg(cfg->mode);
    if (pincfg == NULL) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    uint32_t status = am_hal_gpio_pinconfig(cfg->pin, *pincfg);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    if (cfg->mode == NSX_GPIO_MODE_OUTPUT || cfg->mode == NSX_GPIO_MODE_OUTPUT_WITH_READ) {
        return nsx_gpio_write(cfg->pin, cfg->initial_level);
    }

    return status;
}

uint32_t
nsx_gpio_write(uint32_t pin, nsx_gpio_level_t level)
{
    return am_hal_gpio_state_write(
        pin, level == NSX_GPIO_LEVEL_HIGH ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
}

uint32_t
nsx_gpio_read(uint32_t pin, nsx_gpio_level_t *level)
{
#ifndef NSX_DISABLE_API_VALIDATION
    if (level == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
#endif

    uint32_t state = 0;
    uint32_t status = am_hal_gpio_state_read(pin, AM_HAL_GPIO_INPUT_READ, &state);
    if (status != AM_HAL_STATUS_SUCCESS) {
        return status;
    }

    *level = state ? NSX_GPIO_LEVEL_HIGH : NSX_GPIO_LEVEL_LOW;
    return status;
}

uint32_t
nsx_gpio_toggle(uint32_t pin)
{
    return am_hal_gpio_state_write(pin, AM_HAL_GPIO_OUTPUT_TOGGLE);
}

uint32_t
nsx_gpio_irq_enable(uint32_t pin)
{
    if (pin >= AM_HAL_GPIO_MAX_PADS) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    uint32_t gpio_num = pin;
    return am_hal_gpio_interrupt_control(
        AM_HAL_GPIO_INT_CHANNEL_0, AM_HAL_GPIO_INT_CTRL_INDV_ENABLE, &gpio_num);
}

uint32_t
nsx_gpio_irq_disable(uint32_t pin)
{
    if (pin >= AM_HAL_GPIO_MAX_PADS) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    uint32_t gpio_num = pin;
    return am_hal_gpio_interrupt_control(
        AM_HAL_GPIO_INT_CHANNEL_0, AM_HAL_GPIO_INT_CTRL_INDV_DISABLE, &gpio_num);
}

uint32_t
nsx_gpio_irq_clear(uint32_t pin)
{
    if (pin >= AM_HAL_GPIO_MAX_PADS) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    IRQn_Type irqn = nsx_gpio_pin_to_irqn(pin);
    return am_hal_gpio_interrupt_irq_clear(irqn, 1u << (pin & 0x1Fu));
}
