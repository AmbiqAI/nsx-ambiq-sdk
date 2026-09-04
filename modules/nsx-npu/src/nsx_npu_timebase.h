/**
 * @file nsx_npu_timebase.h
 * @author Ambiq
 * @brief nsx-npu internal: bring-up for the Ethos-U wait-semaphore timebase.
 *
 * Not part of the module's public API (`includes-api/nsx_npu.h`). The strong
 * `nsx_ethos_u_ticks()` / `nsx_ethos_u_ticks_per_ms()` overrides themselves are
 * declared by nsx-ethos-u-driver in `nsx_ethos_u.h`; only the SoC-specific
 * bring-up entry point lives here. The status enum it returns is public (see
 * `nsx_npu_timebase_status()` in `nsx_npu.h`) so applications can confirm the
 * bounded inference wait is armed.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#ifndef NSX_NPU_TIMEBASE_H
#define NSX_NPU_TIMEBASE_H

#include "nsx_npu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up the tick source backing the Ethos-U wait semaphore.
 *
 * Idempotent once armed; `nsx_npu_timebase_deinit()` disarms it again. Must run
 * before the first inference is dispatched; `nsx_npu_init` calls it. Until it
 * has returned `NSX_NPU_TIMEBASE_ARMED`, `nsx_ethos_u_ticks_per_ms()` reports 0
 * and every `ethosu_wait()` degrades to upstream's unbounded wait. A non-armed
 * result may be retried by calling again (for example after the application has
 * reconfigured STIMER).
 *
 * @return The resulting timebase status; also readable later through
 *         `nsx_npu_timebase_status()`.
 */
nsx_npu_timebase_status_e nsx_npu_timebase_init(void);

/**
 * @brief Tear the tick source back down to its pre-init state.
 *
 * Disarms the timebase so the next `nsx_npu_timebase_init()` re-derives the
 * rate and re-runs the liveness probe against whatever STIMER looks like then,
 * rather than trusting the cached ARMED result. Does not stop STIMER
 * (`nsx_npu_deinit()` leaves it running by design). Safe to call when the
 * timebase was never armed; `nsx_npu_deinit()` calls it.
 */
void nsx_npu_timebase_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // NSX_NPU_TIMEBASE_H
