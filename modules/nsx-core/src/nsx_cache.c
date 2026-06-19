/**
 * @file nsx_cache.c
 * @brief I/D cache control and coherence helpers for the nsx-mem subsystem.
 *
 * Provides lightweight cache enable/disable (no full nsx-power teardown) plus
 * guarantee-named coherence helpers (publish writes / invalidate observed data
 * / sync shared data). Each guarantee is gated by an NSX_CACHE_HAS_* capability
 * macro and returns NSX_CACHE_UNSUPPORTED where the family cannot honor it.
 * Apollo2/3/4 expose a unified controller; Apollo5-family parts expose split
 * I/D cache control with explicit clean/invalidate. Declared in nsx_mem.h.
 */

#include "nsx_mem.h"

#include "am_mcu_apollo.h"

uint32_t nsx_cache_enable(void)
{
#if defined(AM_PART_APOLLO2)
    am_hal_cachectrl_enable(&am_hal_cachectrl_defaults);
    return 0;
#elif defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();
    return 0;
#elif defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
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
#if defined(AM_PART_APOLLO2) || defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    am_hal_cachectrl_disable();
#elif defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    am_hal_cachectrl_dcache_disable();
    am_hal_cachectrl_icache_disable();
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4)
    am_hal_cachectrl_disable();
#endif
}

uint32_t nsx_cache_flush(void)
{
    /*
     * Backward-compatible alias of nsx_cache_publish_writes(). Retains the
     * historical no-op-returns-success contract on parts without a public
     * bus-flush primitive (e.g. Apollo2).
     */
#if NSX_CACHE_HAS_PUBLISH_WRITES
    return nsx_cache_publish_writes();
#else
    return 0;
#endif
}

uint32_t nsx_cache_publish_writes(void)
{
    /* Make prior CPU writes visible to other bus masters (device -> host). */
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    return am_hal_cachectrl_dcache_clean(NULL);
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4)
    /* DAXI flush drains (and invalidates) the deep write buffer. */
    return am_hal_sysctrl_bus_write_flush();
#elif defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    /* SYNC_READ drains buffered writes; the read value carries no status. */
    (void)am_hal_sysctrl_bus_write_flush();
    return 0;
#else
    return NSX_CACHE_UNSUPPORTED;
#endif
}

uint32_t nsx_cache_invalidate_observed_data(void)
{
    /*
     * Discard stale CPU copies so the next read observes external writes
     * (host/DMA -> device). Only AP5-class parts expose a real data-cache
     * invalidate. On AP2/AP3/AP4 data memory is not CPU read-cached (AP4 DAXI
     * is a write buffer, not a read cache), so the guarantee is unsupported.
     */
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    return am_hal_cachectrl_dcache_invalidate(NULL, false);
#else
    return NSX_CACHE_UNSUPPORTED;
#endif
}

uint32_t nsx_cache_sync_shared_data(void)
{
    /* Conservative bidirectional sync point for a shared buffer. */
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
    /* Clean + invalidate: publish writes and drop stale lines in one step. */
    return am_hal_cachectrl_dcache_invalidate(NULL, true);
#elif defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4)
    /* DAXI flush already performs flush + invalidate of the write buffer. */
    return am_hal_sysctrl_bus_write_flush();
#else
    return NSX_CACHE_UNSUPPORTED;
#endif
}
