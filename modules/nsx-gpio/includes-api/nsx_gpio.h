#ifndef NSX_GPIO_H
#define NSX_GPIO_H

// Ambiq/CMSIS headers include overloaded MVE intrinsics in C++ mode.
// Keep them out of any caller-provided extern "C" block.
#ifdef __cplusplus
extern "C++" {
#endif
#include "am_mcu_apollo.h"
#include "am_util.h"
#ifdef __cplusplus
}
#endif

#include "nsx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NSX_GPIO_V0_0_1 { .major = 0, .minor = 0, .revision = 1 }
#define NSX_GPIO_OLDEST_SUPPORTED_VERSION NSX_GPIO_V0_0_1
#define NSX_GPIO_CURRENT_VERSION NSX_GPIO_V0_0_1
#define NSX_GPIO_API_ID 0xCA0010

extern const nsx_core_api_t nsx_gpio_V0_0_1;
extern const nsx_core_api_t nsx_gpio_oldest_supported_version;
extern const nsx_core_api_t nsx_gpio_current_version;

typedef enum {
    NSX_GPIO_MODE_DISABLED = 0,
    NSX_GPIO_MODE_INPUT = 1,
    NSX_GPIO_MODE_OUTPUT = 2,
    NSX_GPIO_MODE_OUTPUT_WITH_READ = 3,
} nsx_gpio_mode_t;

typedef enum {
    NSX_GPIO_LEVEL_LOW = 0,
    NSX_GPIO_LEVEL_HIGH = 1,
} nsx_gpio_level_t;

//
// Edge triggers for pin interrupts. The Apollo GPIO block is edge-sensitive
// (INTDIR), so only rising/falling/both edges are supported.
//
typedef enum {
    NSX_GPIO_TRIGGER_NONE = 0,
    NSX_GPIO_TRIGGER_RISING = 1,
    NSX_GPIO_TRIGGER_FALLING = 2,
    NSX_GPIO_TRIGGER_BOTH = 3,
} nsx_gpio_trigger_t;

//! Callback invoked (in interrupt context) when a configured pin fires.
typedef void (*nsx_gpio_irq_cb_t)(uint32_t pin, void *ctx);

typedef struct {
    const nsx_core_api_t *api;
    uint32_t pin;
    nsx_gpio_mode_t mode;
    nsx_gpio_level_t initial_level;

    // Interrupt configuration (ignored when trigger == NSX_GPIO_TRIGGER_NONE).
    nsx_gpio_trigger_t trigger; //!< Edge that raises the interrupt.
    nsx_gpio_irq_cb_t irq_cb;   //!< Callback for this pin.
    void *irq_ctx;              //!< Opaque context passed to the callback.
} nsx_gpio_config_t;

uint32_t nsx_gpio_init(const nsx_gpio_config_t *cfg);
uint32_t nsx_gpio_write(uint32_t pin, nsx_gpio_level_t level);
uint32_t nsx_gpio_read(uint32_t pin, nsx_gpio_level_t *level);
uint32_t nsx_gpio_toggle(uint32_t pin);

//
// Pin interrupt control. A pin's callback and trigger are installed via
// nsx_gpio_init() with a non-NONE trigger; these helpers toggle and clear the
// interrupt afterwards.
//
uint32_t nsx_gpio_irq_enable(uint32_t pin);
uint32_t nsx_gpio_irq_disable(uint32_t pin);
uint32_t nsx_gpio_irq_clear(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif
