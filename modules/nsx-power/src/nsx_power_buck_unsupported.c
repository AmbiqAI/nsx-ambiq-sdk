#include "nsx_power_buck.h"

uint32_t nsx_power_buck_rails(void) { return 0; }
nsx_buck_status_t nsx_power_buck_start(const nsx_power_buck_session_t *session) {
    (void)session;
    return NSX_BUCK_UNSUPPORTED;
}
nsx_buck_status_t nsx_power_buck_read(const nsx_power_buck_session_t *session,
                                      nsx_power_buck_sample_t *sample) {
    (void)session;
    (void)sample;
    return NSX_BUCK_UNSUPPORTED;
}
nsx_buck_status_t nsx_power_buck_stop(const nsx_power_buck_session_t *session) {
    (void)session;
    return NSX_BUCK_UNSUPPORTED;
}
