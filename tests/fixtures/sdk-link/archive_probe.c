#include "am_bsp.h"
#include "am_mcu_apollo.h"

uint32_t nsx_sdk_archive_probe(void)
{
    am_hal_pdm_config_t config = {0};
    am_bsp_low_power_init();
    return am_hal_pdm_configure(0, &config);
}
