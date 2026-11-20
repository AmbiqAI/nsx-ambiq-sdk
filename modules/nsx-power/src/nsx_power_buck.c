#include "nsx_power_buck_internal.h"

#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    uint32_t tick;
    uint32_t hz;
    uint32_t counts[NSX_BUCK_RAIL_COUNT];
    nsx_buck_modes_t modes;
} counter_snapshot_t;

static atomic_flag lock = ATOMIC_FLAG_INIT;
static const nsx_power_buck_session_t *owner;
static nsx_power_buck_session_t config;
static uint32_t touched_rails;
static counter_snapshot_t baseline;
static nsx_buck_status_t read_status;

static nsx_buck_status_t release_timers(void) {
    read_status = NSX_BUCK_CLEANUP_REQUIRED;
    for (unsigned r = 0; r < NSX_BUCK_RAIL_COUNT; ++r) {
        uint32_t bit = NSX_BUCK_RAIL(r);
        if ((touched_rails & bit) && nsx_buck_hw_stop(config.timer[r]) == NSX_BUCK_OK) {
            touched_rails &= ~bit;
        }
    }
    if (touched_rails) return NSX_BUCK_CLEANUP_REQUIRED;
    owner = NULL;
    return NSX_BUCK_OK;
}

static nsx_buck_status_t validate(const nsx_power_buck_session_t *session) {
    if (session == NULL || session->rails == 0) return NSX_BUCK_INVALID;
    if (session->rails & ~nsx_power_buck_rails()) return NSX_BUCK_UNSUPPORTED;
    for (unsigned r = 0; r < NSX_BUCK_RAIL_COUNT; ++r) {
        if (!(session->rails & NSX_BUCK_RAIL(r))) continue;
        for (unsigned prior = 0; prior < r; ++prior) {
            if ((session->rails & NSX_BUCK_RAIL(prior)) && session->timer[prior] == session->timer[r]) {
                return NSX_BUCK_INVALID;
            }
        }
        nsx_buck_status_t status = nsx_buck_hw_check((nsx_buck_rail_t)r, session->timer[r]);
        if (status != NSX_BUCK_OK) return status;
    }
    return NSX_BUCK_OK;
}

static nsx_buck_status_t snapshot(counter_snapshot_t *out) {
    out->hz = nsx_buck_hw_time(&out->tick);
    if (!out->hz) return NSX_BUCK_NO_TIMEBASE;
    out->modes = nsx_buck_hw_modes();
    for (unsigned r = 0; r < NSX_BUCK_RAIL_COUNT; ++r) {
        if ((config.rails & NSX_BUCK_RAIL(r)) && !nsx_buck_hw_count(config.timer[r], &out->counts[r])) {
            return NSX_BUCK_INTERRUPTED;
        }
    }
    return NSX_BUCK_OK;
}

static nsx_buck_status_t start_locked(const nsx_power_buck_session_t *session) {
    if (owner) return NSX_BUCK_BUSY;
    nsx_buck_status_t status = validate(session);
    if (status != NSX_BUCK_OK) return status;
    uint32_t tick;
    if (!nsx_buck_hw_time(&tick)) return NSX_BUCK_NO_TIMEBASE;
    config = *session;
    owner = session;
    for (unsigned r = 0; r < NSX_BUCK_RAIL_COUNT; ++r) {
        if (!(config.rails & NSX_BUCK_RAIL(r))) continue;
        bool touched = false;
        status = nsx_buck_hw_start((nsx_buck_rail_t)r, config.timer[r], &touched);
        if (touched) touched_rails |= NSX_BUCK_RAIL(r);
        if (status != NSX_BUCK_OK) break;
    }
    if (status == NSX_BUCK_OK) status = snapshot(&baseline);
    if (status != NSX_BUCK_OK) {
        nsx_buck_status_t cleanup = release_timers();
        return cleanup == NSX_BUCK_OK ? status : cleanup;
    }
    read_status = NSX_BUCK_OK;
    return NSX_BUCK_OK;
}

nsx_buck_status_t nsx_power_buck_start(const nsx_power_buck_session_t *session) {
    if (atomic_flag_test_and_set(&lock)) return NSX_BUCK_BUSY;
    nsx_buck_status_t status = start_locked(session);
    atomic_flag_clear(&lock);
    return status;
}

static nsx_buck_status_t read_locked(const nsx_power_buck_session_t *session,
                                     nsx_power_buck_sample_t *sample) {
    if (session == NULL || sample == NULL) return NSX_BUCK_INVALID;
    if (session != owner) return NSX_BUCK_NOT_ACTIVE;
    if (read_status != NSX_BUCK_OK) return read_status;
    counter_snapshot_t end;
    read_status = snapshot(&end);
    if (read_status != NSX_BUCK_OK) return read_status;
    if (end.hz != baseline.hz) {
        read_status = NSX_BUCK_NO_TIMEBASE;
        return read_status;
    }
    *sample = (nsx_power_buck_sample_t){
        .rails = config.rails,
        .elapsed_ticks = end.tick - baseline.tick,
        .tick_hz = end.hz,
        .start_modes = baseline.modes,
        .end_modes = end.modes,
    };
    for (unsigned r = 0; r < NSX_BUCK_RAIL_COUNT; ++r) {
        if (config.rails & NSX_BUCK_RAIL(r)) sample->pulses[r] = end.counts[r] - baseline.counts[r];
    }
    return NSX_BUCK_OK;
}

nsx_buck_status_t nsx_power_buck_read(const nsx_power_buck_session_t *session,
                                      nsx_power_buck_sample_t *sample) {
    if (atomic_flag_test_and_set(&lock)) return NSX_BUCK_BUSY;
    nsx_buck_status_t status = read_locked(session, sample);
    atomic_flag_clear(&lock);
    return status;
}

nsx_buck_status_t nsx_power_buck_stop(const nsx_power_buck_session_t *session) {
    if (session == NULL) return NSX_BUCK_INVALID;
    if (atomic_flag_test_and_set(&lock)) return NSX_BUCK_BUSY;
    nsx_buck_status_t status = session == owner ? release_timers() : NSX_BUCK_NOT_ACTIVE;
    atomic_flag_clear(&lock);
    return status;
}
