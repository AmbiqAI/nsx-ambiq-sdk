#include "nsx_debug_internal.h"

extern uint32_t nsx_platform_debug_init(const nsx_debug_config_t *cfg);
extern uint32_t nsx_platform_debug_disable(nsx_debug_transport_e transport);

/* Application-serialized ownership; raw BSP/HAL users are outside this state. */
static nsx_debug_transport_e s_active;
static nsx_debug_transport_e s_selected = NSX_DEBUG_ITM;
static uint32_t s_error;

bool nsx_debug_output_enabled(void) {
    return s_active != NSX_DEBUG_NONE && s_error == 0;
}

uint32_t nsx_debug_before_hw_init(void) {
    nsx_debug_suspend();
    return s_error;
}

void nsx_debug_disable(nsx_debug_transport_e transport) {
    if (transport != s_active || s_active == NSX_DEBUG_NONE || s_error != 0) return;
    s_active = NSX_DEBUG_NONE;
    s_error = nsx_platform_debug_disable(transport);
}

void nsx_debug_suspend(void) {
    nsx_debug_disable(s_active);
}

uint32_t nsx_debug_init(const nsx_debug_config_t *cfg) {
    if (cfg == NULL || cfg->transport == NSX_DEBUG_NONE) return 0;
    if (cfg->transport != NSX_DEBUG_ITM && cfg->transport != NSX_DEBUG_UART) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (s_error != 0) return s_error;
    if (cfg->transport == s_active) return 0;
    nsx_debug_suspend();
    if (s_error != 0) return s_error;
    s_error = nsx_platform_debug_init(cfg);
    if (s_error == 0) {
        s_active = cfg->transport;
        s_selected = cfg->transport;
    }
    return s_error;
}

void nsx_debug_resume(void) {
    nsx_debug_config_t cfg = {.transport = s_selected};
    (void)nsx_debug_init(&cfg);
}
