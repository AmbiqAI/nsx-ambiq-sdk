#include "nsx_startup_internal.h"

#if NSX_STARTUP_GUARDED
#include "am_mcu_apollo.h"
#include "nsx_core.h"

typedef enum {
    NSX_STARTUP_UNCLAIMED,
    NSX_STARTUP_RUNNING,
    NSX_STARTUP_COMPLETE,
    NSX_STARTUP_FAILED,
} nsx_startup_state_t;

static volatile nsx_startup_state_t s_startup;

uint32_t nsx_startup_claim(void) {
    uint32_t mask = am_hal_interrupt_master_disable();
    uint32_t status = NSX_STATUS_STARTUP_REJECTED;
    if (s_startup == NSX_STARTUP_UNCLAIMED) {
        s_startup = NSX_STARTUP_RUNNING;
        status = NSX_STATUS_SUCCESS;
    }
    am_hal_interrupt_master_set(mask);
    return status;
}

uint32_t nsx_startup_finish(uint32_t status) {
    uint32_t mask = am_hal_interrupt_master_disable();
    s_startup = status == NSX_STATUS_SUCCESS ? NSX_STARTUP_COMPLETE : NSX_STARTUP_FAILED;
    am_hal_interrupt_master_set(mask);
    return status;
}
#endif
