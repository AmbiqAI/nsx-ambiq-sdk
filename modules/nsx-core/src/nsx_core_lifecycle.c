/**
 * @file nsx_core.c
 * @author Ambiq
 * @brief Code common to all NSX
 * @version 0.1
 * @date 2022-11-03
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 */
#include "nsx_core.h"

/* Core lifecycle state is private to this translation unit. The core layer
 * owns only API/version validation and the initialized flag — it deliberately
 * holds no debug, crypto, or peripheral policy. */
static bool s_nsx_core_initialized = false;

//*** API Versions
#define NSX_CORE_API_ID 0xCA0000

const nsx_core_api_t nsx_core_V0_0_1 = {.apiId = NSX_CORE_API_ID, .version = NSX_CORE_V0_0_1};

const nsx_core_api_t nsx_core_V1_0_0 = {.apiId = NSX_CORE_API_ID, .version = NSX_CORE_V1_0_0};

const nsx_core_api_t nsx_core_oldest_supported_version = {
    .apiId = NSX_CORE_API_ID, .version = NSX_CORE_V0_0_1};

const nsx_core_api_t nsx_core_current_version = {.apiId = NSX_CORE_API_ID, .version = NSX_CORE_V1_0_0};
//***

/**
 * Compare semantic versions
 *
 * param c - 'current' version
 * param n - 'target' version
 * return int - 0 if equal, -1 is c is less than n, 1 if c is more than n
 */
static int semver_compare(const nsx_semver_t *c, const nsx_semver_t *n) {
    uint64_t cMajor = (uint64_t)c->major;
    uint64_t cMinor = (uint64_t)c->minor;
    uint64_t nMajor = (uint64_t)n->major;
    uint64_t nMinor = (uint64_t)n->minor;
    uint64_t c64 = (cMajor << 32) + (cMinor << 16) + c->revision;
    uint64_t n64 = (nMajor << 32) + (nMinor << 16) + n->revision;
    if (c64 == n64) {
        return 0;
    } else if (c64 < n64) {
        // nsx_printf("c64 < n64 c64: %d, n64: %d\n", c64, n64);
        return -1;
    } else {
        // nsx_printf("c64 > n64 c64: %d, n64: %d\n", c64, n64);
        return 1;
    }
}

/**
 * brief Checks API and semantic version of desired API vs. allowed APIs
 *
 * param submitted
 * param oldest
 * param newest
 * return uint32_t
 */
extern uint32_t nsx_core_check_api(
    const nsx_core_api_t *submitted, const nsx_core_api_t *oldest, const nsx_core_api_t *newest) {

    // nsx_printf("submitted->apiId: %x\n", submitted->apiId);
    // nsx_printf("submitted->major: %d\n", submitted->version.major);
    // nsx_printf("submitted->minor: %d\n", submitted->version.minor);
    // nsx_printf("submitted->revision: %d\n", submitted->version.revision);
    // nsx_printf("oldest->apiId: %x\n", oldest->apiId);
    // nsx_printf("oldest->major: %d\n", oldest->version.major);
    // nsx_printf("oldest->minor: %d\n", oldest->version.minor);
    // nsx_printf("oldest->revision: %d\n", oldest->version.revision);
    // nsx_printf("newest->apiId: %x\n", newest->apiId);
    // nsx_printf("newest->major: %d\n", newest->version.major);
    // nsx_printf("newest->minor: %d\n", newest->version.minor);
    // nsx_printf("newest->revision: %d\n", newest->version.revision);
    // nsx_printf("newest->apiId: %x\n", newest->apiId);
    if (submitted->apiId != newest->apiId) {
        return NSX_STATUS_INVALID_VERSION;
    }

    // check version > oldest and < newest
    int resolution = semver_compare(&(submitted->version), &(oldest->version));
    if (resolution == -1) {
        // submitted version is lower than oldest supported version
        return NSX_STATUS_INVALID_VERSION;
    }
    resolution = semver_compare(&(submitted->version), &(newest->version));
    if (resolution == 1) {
        // submitted version is higher than newest supported version
        return NSX_STATUS_INVALID_VERSION;
    }

    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_core_init(nsx_core_config_t *cfg) {
#ifndef NSX_DISABLE_API_VALIDATION
    //
    // Check the handle.
    //
    if (cfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    // check API version
    if (nsx_core_check_api(cfg->api, &nsx_core_oldest_supported_version, &nsx_core_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif
    s_nsx_core_initialized = true;
    return NSX_STATUS_SUCCESS;
}

bool nsx_core_initialized(void) {
    return s_nsx_core_initialized;
}

void nsx_core_fail_loop() {
    while (1)
        ;
}
