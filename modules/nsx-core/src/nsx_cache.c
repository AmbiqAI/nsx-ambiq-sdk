/**
 * @file nsx_cache.c
 * @brief I/D cache control for the nsx-mem memory subsystem.
 *
 * Lightweight cache enable/disable that does not require the full
 * nsx-power teardown. Declared in nsx_mem.h. Apollo5-family parts expose a
 * split I/D cache controller; Apollo4 exposes a single unified controller.
 * Everything else is a no-op.
 */

#include "nsx_mem.h"

#include "am_mcu_apollo.h"

uint32_t nsx_cache_enable(void)
{
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    uint32_t status = am_hal_cachectrl_icache_enable();
    if (status != 0) {
        return status;
    }
    return am_hal_cachectrl_dcache_enable(true);
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4)
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();
    return 0;
#else
    return 0;
#endif
}

void nsx_cache_disable(void)
{
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    am_hal_cachectrl_dcache_disable();
    am_hal_cachectrl_icache_disable();
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4)
    am_hal_cachectrl_disable();
#endif
}
