#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_power.h"
#include "unity.h"
#include "result_reporter.h"

void setUp(void) {}
void tearDown(void) {}

static void initialize_once(void) {
    nsx_system_config_t system = {.enable_cache = true};
    uint32_t mask = __get_PRIMASK();
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    TEST_ASSERT_EQUAL_UINT32(mask, __get_PRIMASK());
}

static void reject_reapplication(void) {
    nsx_system_config_t system = {0};
    uint32_t mask = __get_PRIMASK();
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_system_init(&system));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_hw_init());
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_minimal_hw_init());
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_power_configure(&nsx_power_all_on));
    TEST_ASSERT_EQUAL_UINT32(mask, __get_PRIMASK());
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(initialize_once);
    RUN_TEST(reject_reapplication);
    nsx_test_finish();
}
