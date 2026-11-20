#include "nsx_power_status.h"

#if defined(NSX_POWER_STATUS_APOLLO3) || defined(NSX_POWER_STATUS_HAL)
#include "am_mcu_apollo.h"
#endif

nsx_power_status_result_t nsx_power_status_read(nsx_power_status_t *out)
{
    if (!out) {
        return NSX_POWER_STATUS_INVALID;
    }

#if defined(NSX_POWER_STATUS_APOLLO3)
    nsx_power_status_t snapshot = {
        .valid = NSX_POWER_STATUS_VALID_DEVICE | NSX_POWER_STATUS_VALID_MCU_MEMORY |
                 NSX_POWER_STATUS_VALID_SUPPLY
    };
    snapshot.device = PWRCTRL->DEVPWRSTATUS;
    snapshot.mcu_memory = PWRCTRL->MEMPWRSTATUS;
    snapshot.supply = PWRCTRL->SUPPLYSTATUS;
    *out = snapshot;
    return NSX_POWER_STATUS_OK;
#elif defined(NSX_POWER_STATUS_HAL)
    am_hal_pwrctrl_status_t status;
    if (am_hal_pwrctrl_status_get(&status) != AM_HAL_STATUS_SUCCESS) {
        return NSX_POWER_STATUS_HAL_ERROR;
    }
    *out = (nsx_power_status_t){
        .valid = NSX_POWER_STATUS_VALID_DEVICE | NSX_POWER_STATUS_VALID_MCU_MEMORY |
                 NSX_POWER_STATUS_VALID_SHARED_SRAM | NSX_POWER_STATUS_VALID_SYSTEM |
                 NSX_POWER_STATUS_VALID_AUDIO | NSX_POWER_STATUS_VALID_REGULATOR,
        .device = status.ui32Device,
        .mcu_memory = status.ui32Memory,
        .shared_sram = status.ui32SSRAM,
        .system = status.ui32System,
        .audio = status.ui32AudioSS,
        .regulator = status.ui32VRStatus
    };
    return NSX_POWER_STATUS_OK;
#else
    return NSX_POWER_STATUS_UNSUPPORTED;
#endif
}
