#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "unity.h"
#include "identity.h"
#include "result_reporter.h"

static volatile uint32_t data_probe = 0x12345678;
static volatile uint32_t bss_probe;
static volatile uint32_t constructor_probe;
struct ConstructorProbe {
    ConstructorProbe() { constructor_probe = data_probe ^ 0xFFFFFFFFU; }
};
static ConstructorProbe constructor;
static char transcript[4096];
static unsigned length, overflow;
static uint32_t entry_debug, entry_mask, after_mask, init_status;

extern "C" {
void setUp(void) {}
void tearDown(void) {}
void __real_nsx_test_putchar(int ch);
void __wrap_nsx_test_putchar(int ch) {
    __real_nsx_test_putchar(ch);
    if (length < sizeof(transcript) - 1) transcript[length++] = (char)ch;
    else overflow = 1;
}
}
static void initialized_data(void) { TEST_ASSERT_EQUAL_HEX32(0x12345678, data_probe); }
static void zeroed_bss(void) { TEST_ASSERT_EQUAL_UINT32(0, bss_probe); }
static void cpp_constructor(void) { TEST_ASSERT_EQUAL_HEX32(0xEDCBA987, constructor_probe); }
static void debugger_detached(void) { TEST_ASSERT_EQUAL_UINT32(0, entry_debug); }
static void system_startup(void) { TEST_ASSERT_EQUAL_UINT32(0, init_status); }
static void interrupt_mask(void) {
#if defined(AM_PART_APOLLO4P)
    TEST_ASSERT_EQUAL_UINT32(0, entry_mask);
#endif
    TEST_ASSERT_EQUAL_UINT32(entry_mask, after_mask);
}

int main(void) {
    entry_debug = (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0;
    entry_mask = __get_PRIMASK();
    nsx_system_config_t cfg = {};
    cfg.enable_cache = true;
    cfg.debug.transport = NSX_DEBUG_UART;
    init_status = nsx_system_init(&cfg);
    after_mask = __get_PRIMASK();
    nsx_test_begin(__FILE__);
    RUN_TEST(initialized_data);
    RUN_TEST(zeroed_bss);
    RUN_TEST(cpp_constructor);
    RUN_TEST(debugger_detached);
    RUN_TEST(system_startup);
    RUN_TEST(interrupt_mask);
    UNITY_END();
    transcript[length] = '\0';
    // Replay the result, not the tests, while the USB UART bridge enumerates.
    for (;;) {
        nsx_low_power_printf("\nNSX_BOOT_BEGIN %s %u %u %u %u\n", NSX_TEST_BUILD_ID,
                             entry_debug, entry_mask, after_mask, overflow);
        for (unsigned i = 0; i < length; ++i) nsx_low_power_printf("%c", transcript[i]);
        nsx_low_power_printf("NSX_BOOT_END %s %u %u %u\n", NSX_TEST_BUILD_ID,
                             Unity.NumberOfTests, Unity.TestFailures, Unity.TestIgnores);
        nsx_delay_us(1000000);
    }
}
