#ifndef NSX_TEST_POWER_STATUS_HAL_H
#define NSX_TEST_POWER_STATUS_HAL_H

#include <stdint.h>

#if defined(NSX_POWER_STATUS_APOLLO3)
typedef struct {
    uint32_t DEVPWRSTATUS;
    uint32_t MEMPWRSTATUS;
    uint32_t SUPPLYSTATUS;
} fake_pwrctrl_t;
extern const volatile fake_pwrctrl_t fixture_registers;
#define PWRCTRL (&fixture_registers)
#elif defined(NSX_POWER_STATUS_HAL)
#define AM_HAL_STATUS_SUCCESS 0
typedef struct {
    uint32_t ui32Device, ui32AudioSS, ui32Memory, ui32System, ui32SSRAM, ui32Audio, ui32VRStatus;
} am_hal_pwrctrl_status_t;
uint32_t am_hal_pwrctrl_status_get(am_hal_pwrctrl_status_t *out);
#else
#error An unsupported target must build without power-control headers.
#endif

#endif
