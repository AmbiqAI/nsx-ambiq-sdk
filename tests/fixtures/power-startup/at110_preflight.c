#include <assert.h>
#include "nsx_power.h"

static unsigned debug_calls, platform_calls;
uint32_t nsx_debug_before_hw_init(void) { ++debug_calls; return NSX_STATUS_SUCCESS; }
uint32_t nsx_power_platform_config(const nsx_power_config_t *cfg) {
    assert(!cfg->small_tcm && cfg->need_ssram);
    ++platform_calls;
    return NSX_STATUS_SUCCESS;
}
void nsx_power_platform_deep_sleep(void) {}
uint32_t nsx_core_check_api(const nsx_core_api_t *api, const nsx_core_api_t *oldest,
                            const nsx_core_api_t *latest) {
    (void)oldest;
    return api->apiId != latest->apiId;
}

int main(void) {
    for (unsigned small = 0; small <= 1; ++small) {
        for (unsigned shared = 0; shared <= 1; ++shared) {
            nsx_power_config_t cfg = nsx_power_all_on;
            cfg.small_tcm = small;
            cfg.need_ssram = shared;
            debug_calls = platform_calls = 0;
            uint32_t expected = !small && shared ? NSX_STATUS_SUCCESS : NSX_STATUS_INVALID_CONFIG;
            assert(nsx_power_configure(&cfg) == expected);
            assert(debug_calls == (expected == NSX_STATUS_SUCCESS));
            assert(platform_calls == debug_calls);
        }
    }
}
