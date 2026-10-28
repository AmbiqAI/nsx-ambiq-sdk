/* Link-only probe: retain public calls without executing destructive helpers. */
#include "nsx_power.h"

volatile unsigned power_probe;
int main(void) {
    if (power_probe) {
        power_probe = nsx_power_configure(&nsx_power_all_on);
        power_probe = nsx_power_set_performance_mode(NSX_POWER_PERF_LOW);
        power_probe = nsx_power_profile_dump(0, NSX_POWER_PROFILE_FORMAT_JSON);
#if !defined(AM_PART_APOLLO330P)
        power_probe = nsx_power_shutdown_peripherals();
        power_probe = nsx_power_minimize_memory();
        nsx_power_disable_nvm();
        nsx_power_disable_caches();
        nsx_power_disable_debug();
        nsx_power_tristate_gpios(0, 0);
#endif
    }
    for (;;) { __asm volatile("nop"); }
}
