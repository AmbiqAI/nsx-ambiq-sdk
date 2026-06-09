/**
 * @brief External power monitor GPIO helpers.
 */

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_power.h"

static bool s_nsx_power_monitor_enabled;

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P) || defined(AM_PART_APOLLO4) || \
    defined(AM_PART_APOLLO4B) || defined(AM_PART_APOLLO4P)
    #define NSX_POWER_MONITOR_GPIO_0 22
    #define NSX_POWER_MONITOR_GPIO_1 23
    #define NSX_POWER_MONITOR_SUPPORTED 1
#elif defined(AM_PART_APOLLO4L)
    #define NSX_POWER_MONITOR_GPIO_0 61
    #define NSX_POWER_MONITOR_GPIO_1 23
    #define NSX_POWER_MONITOR_SUPPORTED 1
#elif defined(AM_PART_APOLLO510B) || defined(AM_PART_APOLLO330P)
    #define NSX_POWER_MONITOR_GPIO_0 0
    #define NSX_POWER_MONITOR_GPIO_1 1
    #define NSX_POWER_MONITOR_SUPPORTED 1
#elif defined(AM_PART_APOLLO510L)
    #define NSX_POWER_MONITOR_GPIO_0 29
    #define NSX_POWER_MONITOR_GPIO_1 31
    #define NSX_POWER_MONITOR_SUPPORTED 1
#elif defined(AM_PART_APOLLO5A) || (defined(AM_PART_APOLLO5B) && !defined(AM_PART_APOLLO510B))
    #define NSX_POWER_MONITOR_GPIO_0 29
    #define NSX_POWER_MONITOR_GPIO_1 36
    #define NSX_POWER_MONITOR_SUPPORTED 1
#else
    #define NSX_POWER_MONITOR_SUPPORTED 0
#endif

uint32_t nsx_power_monitor_init(void) {
#if !NSX_POWER_MONITOR_SUPPORTED
    return NSX_STATUS_INVALID_CONFIG;
#else
    am_hal_gpio_pinconfig(NSX_POWER_MONITOR_GPIO_0, am_hal_gpio_pincfg_output);
    am_hal_gpio_pinconfig(NSX_POWER_MONITOR_GPIO_1, am_hal_gpio_pincfg_output);
    s_nsx_power_monitor_enabled = true;
    nsx_power_monitor_set_state(NSX_POWER_MONITOR_STATE_IDLE);
    return NSX_STATUS_SUCCESS;
#endif
}

void nsx_power_monitor_set_state(nsx_power_monitor_state_t state) {
#if NSX_POWER_MONITOR_SUPPORTED
    if (!s_nsx_power_monitor_enabled) {
        return;
    }

    am_hal_gpio_state_write(NSX_POWER_MONITOR_GPIO_0, state & 0x01u);
    am_hal_gpio_state_write(NSX_POWER_MONITOR_GPIO_1, (state >> 1) & 0x01u);
#else
    (void)state;
#endif
}
