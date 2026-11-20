#include "nsx_power_status.h"

#include <assert.h>
#include <string.h>

#if defined(NSX_POWER_STATUS_HAL)
#include "am_mcu_apollo.h"

static unsigned calls;
static uint32_t hal_result;
static am_hal_pwrctrl_status_t sample;

uint32_t am_hal_pwrctrl_status_get(am_hal_pwrctrl_status_t *out)
{
    ++calls;
    *out = sample;
    return hal_result;
}
#elif defined(NSX_POWER_STATUS_APOLLO3)
#include "am_mcu_apollo.h"
const volatile fake_pwrctrl_t fixture_registers = {
    .DEVPWRSTATUS = 0x40,
    .MEMPWRSTATUS = 0xbbcc,
    .SUPPLYSTATUS = 0x6
};
#endif

int main(void)
{
    nsx_power_status_t out;
    memset(&out, 0xa5, sizeof out);
#if !defined(NSX_POWER_STATUS_APOLLO3)
    nsx_power_status_t before = out;
#endif
    assert(nsx_power_status_read(NULL) == NSX_POWER_STATUS_INVALID);

#if defined(NSX_POWER_STATUS_HAL)
    assert(calls == 0);
    hal_result = 3;
    assert(nsx_power_status_read(&out) == NSX_POWER_STATUS_HAL_ERROR);
    assert(!memcmp(&out, &before, sizeof out));

    hal_result = AM_HAL_STATUS_SUCCESS;
    sample = (am_hal_pwrctrl_status_t){
        .ui32Device = 0x1234, .ui32Memory = 0x56, .ui32SSRAM = 0xab,
        .ui32System = 0xcd, .ui32AudioSS = 0xef, .ui32VRStatus = 0,
        .ui32Audio = 0xdead  /* The pinned getter leaves this member unwritten. */
    };
    assert(nsx_power_status_read(&out) == NSX_POWER_STATUS_OK);
    assert(calls == 2);
    assert(out.valid == (NSX_POWER_STATUS_VALID_DEVICE | NSX_POWER_STATUS_VALID_MCU_MEMORY |
                         NSX_POWER_STATUS_VALID_SHARED_SRAM | NSX_POWER_STATUS_VALID_SYSTEM |
                         NSX_POWER_STATUS_VALID_AUDIO | NSX_POWER_STATUS_VALID_REGULATOR));
    assert(out.device == 0x1234 && out.mcu_memory == 0x56 && out.shared_sram == 0xab);
    assert(out.system == 0xcd && out.audio == 0xef && out.regulator == 0 && out.supply == 0);
    sample.ui32VRStatus = 0x30;
    assert(nsx_power_status_read(&out) == NSX_POWER_STATUS_OK && out.regulator == 0x30);
#elif defined(NSX_POWER_STATUS_APOLLO3)
    assert(nsx_power_status_read(&out) == NSX_POWER_STATUS_OK);
    assert(out.valid == (NSX_POWER_STATUS_VALID_DEVICE | NSX_POWER_STATUS_VALID_MCU_MEMORY |
                         NSX_POWER_STATUS_VALID_SUPPLY));
    assert(out.device == 0x40 && out.mcu_memory == 0xbbcc && out.supply == 0x6);
    assert(out.shared_sram == 0 && out.system == 0 && out.audio == 0 && out.regulator == 0);
#else
    assert(nsx_power_status_read(&out) == NSX_POWER_STATUS_UNSUPPORTED);
    assert(!memcmp(&out, &before, sizeof out));
#endif
}
