/**
 * @file nsx_npu_timebase.h
 * @author Ambiq
 * @brief nsx-npu internal: bring-up for the Ethos-U wait-semaphore timebase.
 *
 * Not part of the module's public API (`includes-api/nsx_npu.h`). The strong
 * `nsx_ethos_u_ticks()` / `nsx_ethos_u_ticks_per_ms()` overrides themselves are
 * declared by nsx-ethos-u-driver in `nsx_ethos_u.h`; only the SoC-specific
 * bring-up entry point lives here.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#ifndef NSX_NPU_TIMEBASE_H
#define NSX_NPU_TIMEBASE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up the tick source backing the Ethos-U wait semaphore.
 *
 * Idempotent. Must run before the first inference is dispatched; `nsx_npu_init`
 * calls it. Until it has run (or if it determines the tick source is dead),
 * `nsx_ethos_u_ticks_per_ms()` reports 0 and every `ethosu_wait()` degrades to
 * upstream's unbounded wait.
 */
void nsx_npu_timebase_init(void);

#ifdef __cplusplus
}
#endif

#endif // NSX_NPU_TIMEBASE_H
