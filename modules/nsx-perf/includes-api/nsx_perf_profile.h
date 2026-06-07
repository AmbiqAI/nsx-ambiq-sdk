/**
 * @file nsx_cache_profile.h
 * @author Ambiq
 * @brief Collection of cache profiling utilities
 * @version 0.1
 * @date 2022-10-10
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 * \addtogroup nsx-cache-profile
 * @{
 * @ingroup nsx-timer
 *
 */
#ifndef NSX_CACHE_PROF
    #define NSX_CACHE_PROF

    #ifdef __cplusplus
extern "C" {
    #endif

    #include <stdbool.h>
    #include <stdint.h>

/**
 * @brief Struct to capture a snapshot of the cache
 * performance counters
 *
 */
typedef struct {
    uint32_t daccess;
    uint32_t dtaglookup;
    uint32_t dhitslookup;
    uint32_t dhitsline;
    uint32_t iaccess;
    uint32_t itaglookup;
    uint32_t ihitslookup;
    uint32_t ihitsline;
} nsx_cache_dump_t;

/**
 * @brief Struct to configure the cache profiler
 *
 */
typedef struct {
    bool enable;
} nsx_cache_config_t;

/**
 * @brief Initialize the cache profiler
 *
 * @param cfg Configuration struct
 * @return uint8_t
 */
extern uint8_t nsx_cache_profiler_init(nsx_cache_config_t *cfg);

/**
 * @brief Capture the current value of the cache
 * performance counters in the provided struct
 *
 * @param dump
 */
extern void nsx_capture_cache_stats(nsx_cache_dump_t *dump);

/**
 * @brief Print the cache performance counters
 *
 * @param dump
 */
extern void nsx_print_cache_stats(nsx_cache_dump_t *dump);

/**
 * @brief Print the delta between two cache performance
 * counter snapshots
 *
 * @param start
 * @param end
 */
extern void nsx_print_cache_stats_delta(nsx_cache_dump_t *start, nsx_cache_dump_t *end);

/**
 * @brief Capture the delta between two cache performance
 * counter snapshots
 *
 * @param s start snapshot
 * @param e end snapshot
 * @param d struct where delta will be stored
 */
extern void nsx_delta_cache(nsx_cache_dump_t *s, nsx_cache_dump_t *e, nsx_cache_dump_t *d);

typedef struct {
    uint32_t cyccnt;
    uint32_t cpicnt;
    uint32_t exccnt;
    uint32_t sleepcnt;
    uint32_t lsucnt;
    uint32_t foldcnt;
} nsx_perf_counters_t;

/**
 * @brief Reset the HW performance counters
 *
 */
extern void nsx_reset_perf_counters(void);

/**
 * @brief Configure the HW performance capture unit
 *
 */
extern void nsx_init_perf_profiler(void);

/**
 * @brief Start the HW performance capture unit
 *
 */
extern void nsx_start_perf_profiler(void);

/**
 * @brief Stop the HW performance capture unit
 *
 */
extern void nsx_stop_perf_profiler(void);

/**
 * @brief Capture the current value of the HW performance
 * counters in the provided struct
 *
 * @param c
 */
extern void nsx_capture_perf_profiler(nsx_perf_counters_t *c);

/**
 * @brief Calculate the delta between two HW performance snapshots
 *
 * @param s starting snapshot
 * @param e ending snapshot
 * @param d Calculated delta
 */
extern void nsx_delta_perf(nsx_perf_counters_t *s, nsx_perf_counters_t *e, nsx_perf_counters_t *d);

/**
 * @brief Print a HW performance counter snapshot
 *
 * @param c
 */
extern void nsx_print_perf_profile(nsx_perf_counters_t *c);

/**
 * @brief Query whether the target exposes core DWT performance counters.
 *
 * @return true if core counters are available.
 */
extern bool nsx_perf_has_core_counters(void);

/**
 * @brief Query whether the target exposes cache performance counters.
 *
 * @return true if cache counters are available.
 */
extern bool nsx_perf_has_cache_counters(void);

/**
 * @brief Query whether the target exposes PC sampling helpers.
 *
 * @return true if PC sampling is available.
 */
extern bool nsx_perf_has_pcsamp(void);

#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO510L) || defined(AM_PART_APOLLO330P)
uint32_t nsx_perf_enable_pcsamp(void);
int32_t nsx_itm_pcsamp_enable(void);
#endif

#ifdef __cplusplus
}
#endif
#endif // NSX_CACHE_PROF
/** @}*/
