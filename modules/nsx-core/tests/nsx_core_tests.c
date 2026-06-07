#include "nsx_core.h"
#include "unity/unity.h"
#define NSX_CORE_INVALID                                                                         \
        { .major = 5, .minor = 5, .revision = 5 }

void nsx_core_tests_pre_test_hook() {
    // pre hook if needed
}

void nsx_core_tests_post_test_hook() {
    // post hook if needed
}

// Basic test should pass
void nsx_core_test_init() {
    nsx_core_config_t cfg = {
        .api = &nsx_core_V1_0_0
    };
    uint32_t status = nsx_core_init(&cfg);
    TEST_ASSERT_EQUAL(NSX_STATUS_SUCCESS, status);
}

// Test null config
void nsx_core_test_null_cfg() {
    uint32_t status = nsx_core_init(NULL);
    TEST_ASSERT_EQUAL(NSX_STATUS_INVALID_HANDLE, status);
}

// Test different invalid api versions
void nsx_core_test_invalid_api() {
    nsx_core_config_t cfg = {
        .api = NULL
    };
    uint32_t status = nsx_core_init(&cfg);
    TEST_ASSERT_EQUAL(NSX_STATUS_INVALID_VERSION, status);
    // Invalid API id
    nsx_core_api_t invalid_api = {.apiId = 0xCCCCCC, .version = NSX_CORE_V0_0_1};
    cfg.api = &invalid_api;
    uint32_t status2 = nsx_core_init(&cfg);
    TEST_ASSERT_EQUAL(NSX_STATUS_INVALID_VERSION, status2);

    // Invalid API version
    nsx_core_api_t invalid_api2 = {.apiId = 0xCA0000, .version = NSX_CORE_INVALID};
    cfg.api = &invalid_api;
    uint32_t status3 = nsx_core_init(&cfg);
    TEST_ASSERT_EQUAL(NSX_STATUS_INVALID_VERSION, status3);
}