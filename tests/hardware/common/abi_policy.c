#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include <stddef.h>

_Static_assert(sizeof(am_hal_pdm_mclkdiv_e) == 1, "GCC vendor enum ABI");
_Static_assert(sizeof(wchar_t) == 4, "GCC vendor wchar ABI");

/* Retained for link/ABI inspection; firmware never calls this probe. */
uint32_t nsx_test_abi_c(void)
{
    static am_hal_pdm_config_t config;
    am_bsp_low_power_init();
    return am_hal_pdm_configure(0, &config);
}
