#ifndef NSX_POWER_BUCK_H
#define NSX_POWER_BUCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NSX_BUCK_VDDC, NSX_BUCK_VDDF, NSX_BUCK_VDDS, NSX_BUCK_VDDC_LV,
    NSX_BUCK_CPU, NSX_BUCK_MEMORY, NSX_BUCK_BLE, NSX_BUCK_RAIL_COUNT
} nsx_buck_rail_t;

#define NSX_BUCK_RAIL(rail) (UINT32_C(1) << (rail))

typedef enum {
    NSX_BUCK_OK = 0, NSX_BUCK_UNSUPPORTED, NSX_BUCK_INVALID, NSX_BUCK_BUSY,
    NSX_BUCK_NO_TIMEBASE, NSX_BUCK_NOT_ACTIVE, NSX_BUCK_HAL_ERROR, NSX_BUCK_CLEANUP_REQUIRED
} nsx_buck_status_t;

typedef enum {
    NSX_BUCK_SUPPLY_UNKNOWN, NSX_BUCK_SUPPLY_OFF, NSX_BUCK_SUPPLY_ON,
    NSX_BUCK_SUPPLY_LOW_POWER, NSX_BUCK_SUPPLY_ACTIVE
} nsx_buck_supply_t;

typedef enum {
    NSX_BUCK_CPU_UNKNOWN, NSX_BUCK_CPU_NORMAL, NSX_BUCK_CPU_BURST, NSX_BUCK_CPU_ULP,
    NSX_BUCK_CPU_LP, NSX_BUCK_CPU_HP, NSX_BUCK_CPU_HP1, NSX_BUCK_CPU_HP2
} nsx_buck_cpu_t;

typedef struct {
    nsx_buck_supply_t main_supply;
    nsx_buck_supply_t ble_supply;
    nsx_buck_cpu_t cpu_mode;
} nsx_buck_modes_t;

/* Caller-owned session identity. Supply one dedicated hardware timer number per
 * selected rail. Keep this object alive and unchanged through successful stop. */
typedef struct {
    uint32_t rails;
    uint8_t timer[NSX_BUCK_RAIL_COUNT];
} nsx_power_buck_session_t;

typedef struct {
    uint32_t rails;
    uint32_t pulses[NSX_BUCK_RAIL_COUNT];
    uint32_t elapsed_ticks;
    uint32_t tick_hz;
    nsx_buck_modes_t start_modes;
    nsx_buck_modes_t end_modes;
} nsx_power_buck_sample_t;

/* Static mask of routable rail inputs. Zero means unsupported. */
uint32_t nsx_power_buck_rails(void);
/* Foreground/task calls only. One session at a time. If start returns
 * CLEANUP_REQUIRED, retain the session and retry stop. */
nsx_buck_status_t nsx_power_buck_start(const nsx_power_buck_session_t *session);
/* Read deltas since start. Each count and the timebase must advance by less than
 * 2^32 over the window. Reads are sequential. A rail count of zero is valid.
 * Endpoint modes cannot establish whether an intervening transition occurred. */
nsx_buck_status_t nsx_power_buck_read(const nsx_power_buck_session_t *session,
                                      nsx_power_buck_sample_t *sample);
/* Release the session's timers. A cleanup failure retains the session for retry. */
nsx_buck_status_t nsx_power_buck_stop(const nsx_power_buck_session_t *session);

#ifdef __cplusplus
}
#endif
#endif
