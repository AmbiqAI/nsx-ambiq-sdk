#include "unity.h"
#include "result_reporter.h"

volatile unsigned second_seed;
void setUp(void) {}
void tearDown(void) {}
static void check_1(void) { TEST_ASSERT_EQUAL_UINT32(0, second_seed); }
static void check_2(void) { TEST_ASSERT_EQUAL_UINT32(4, sizeof(second_seed)); }
int main(void) {
    nsx_test_begin();
    RUN_TEST(check_1);
    RUN_TEST(check_2);
    nsx_test_finish();
}
