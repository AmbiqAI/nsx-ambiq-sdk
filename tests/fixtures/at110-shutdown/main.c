#include <assert.h>
#include "am_mcu_apollo.h"
#include "nsx_debug_internal.h"
#include "nsx_power.h"

mock_vcomp_t vcomp;
static unsigned calls, fail_at;

static uint32_t result(void) {
    ++calls;
    return calls == fail_at ? 100U + calls : 0;
}

uint32_t nsx_debug_before_hw_init(void) {
    assert(calls == 0 && vcomp.PWDKEY == 0);
    return result();
}

uint32_t am_hal_pwrctrl_control(unsigned control, void *argument) {
    assert(argument == NULL && vcomp.PWDKEY == 0);
    assert((calls == 1 && control == AM_HAL_PWRCTRL_CONTROL_DIS_PERIPHS_ALL) ||
           (calls == 2 && control == AM_HAL_PWRCTRL_CONTROL_XTAL_PWDN_DEEPSLEEP));
    return result();
}

uint32_t am_hal_timer_disable(uint32_t timer) {
    assert(timer < 14 && timer == calls - 3);
    assert(vcomp.PWDKEY == VCOMP_PWDKEY_PWDKEY_Key);
    return result();
}

int main(void) {
    // Debug, two power controls, then timers 0 through 13. Zero selects success.
    const unsigned total = 17;
    for (fail_at = 0; fail_at <= total; ++fail_at) {
        calls = 0;
        vcomp.PWDKEY = 0;
        assert(nsx_power_shutdown_peripherals() == (fail_at ? 100U + fail_at : 0));
        assert(calls == (fail_at ? fail_at : total));
        assert(vcomp.PWDKEY == (fail_at && fail_at <= 3 ? 0 : VCOMP_PWDKEY_PWDKEY_Key));
    }
}
