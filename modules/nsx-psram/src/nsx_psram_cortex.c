#include "nsx_psram_internal.h"
#include "am_mcu_apollo.h"

uint32_t nsx_psram_platform_irq_save(void) {
    uint32_t state = am_hal_interrupt_master_disable();
    __DMB();
    return state;
}

void nsx_psram_platform_irq_restore(uint32_t state) {
    __DMB();
    am_hal_interrupt_master_set(state);
}

void nsx_psram_platform_delay_us(uint32_t us) {
    nsx_delay_us(us);
}
