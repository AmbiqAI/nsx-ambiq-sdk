#include "nsx_usb.h"
#include "nsx_core.h"
#include "unity.h"
#include "result_reporter.h"
void setUp(void) {}
void tearDown(void) {}
static void invalid_handle(void) {
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_HANDLE, nsx_usb_init(NULL));
}
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(invalid_handle);
    nsx_test_finish();
}
