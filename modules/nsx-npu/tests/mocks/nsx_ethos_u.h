/**
 * @file nsx_ethos_u.h
 * @brief Minimal stand-in for nsx-ethos-u-driver's includes-api/nsx_ethos_u.h,
 *        mirroring tools/nsx_npu_compile_check.py's STUB_HEADER: only the two
 *        weak timebase hooks that nsx_npu_timebase.c provides strong
 *        overrides for. Signatures must match the driver contract exactly.
 */
#ifndef NSX_ETHOS_U_H
#define NSX_ETHOS_U_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t nsx_ethos_u_ticks(void);
uint32_t nsx_ethos_u_ticks_per_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NSX_ETHOS_U_H
