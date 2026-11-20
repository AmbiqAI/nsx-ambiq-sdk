#include "nsx_power_buck.h"
#include "result_reporter.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void unsupported_target(void) {
    const nsx_power_buck_session_t session = {.rails = NSX_BUCK_RAIL(NSX_BUCK_VDDC)};
    nsx_power_buck_sample_t sample;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_buck_rails());
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_UNSUPPORTED, nsx_power_buck_start(&session));
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_UNSUPPORTED, nsx_power_buck_read(&session, &sample));
    TEST_ASSERT_EQUAL_UINT32(NSX_BUCK_UNSUPPORTED, nsx_power_buck_stop(&session));
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(unsupported_target);
    nsx_test_finish();
}
