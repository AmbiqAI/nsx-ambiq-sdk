#include <stdint.h>
#include "unity.h"
#include "result_reporter.h"

extern "C" {
volatile uint32_t nsx_probe_data = 0x12345678;
volatile uint32_t nsx_probe_bss;
volatile uint32_t nsx_probe_constructor;
}
struct ConstructorProbe {
    ConstructorProbe() { nsx_probe_constructor = nsx_probe_data ^ 0xFFFFFFFFU; }
};
static ConstructorProbe constructor_probe;

extern "C" {
void setUp(void) {}
void tearDown(void) {}
}
static void initialized_data(void) {
#ifdef NSX_TEST_FAIL_ASSERTION
    TEST_ASSERT_EQUAL_HEX32(0, nsx_probe_data);
#else
    TEST_ASSERT_EQUAL_HEX32(0x12345678, nsx_probe_data);
#endif
}
static void zeroed_bss(void) {
    TEST_ASSERT_EQUAL_HEX32(0, nsx_probe_bss);
}
static void cpp_constructor(void) {
    TEST_ASSERT_EQUAL_HEX32(0xEDCBA987, nsx_probe_constructor);
}
int main(void) {
    nsx_test_begin();
    RUN_TEST(initialized_data);
    RUN_TEST(zeroed_bss);
    RUN_TEST(cpp_constructor);
    nsx_test_finish();
}
