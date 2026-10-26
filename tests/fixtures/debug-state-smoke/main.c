#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "unity.h"
#include "result_reporter.h"

void setUp(void) {}
void tearDown(void) {}

static void output_lifecycle(void) {
    nsx_system_config_t cfg = {.enable_cache=true, .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&cfg));
    nsx_low_power_printf("NSX149:system\n");
    nsx_itm_printf_enable();
    nsx_debug_printf_enable();
    nsx_low_power_printf("NSX149:repeat\n");
    nsx_delay_us(100000);
    nsx_debug_printf_disable();
    TEST_ASSERT_BITS_LOW(ITM_TCR_ITMENA_Msk, ITM->TCR);
    nsx_low_power_printf("NSX149:SHOULD_NOT_PRINT\n");
    nsx_delay_us(100000);
    nsx_debug_printf_enable();
    TEST_ASSERT_BITS_HIGH(ITM_TCR_ITMENA_Msk, ITM->TCR);
    nsx_low_power_printf("NSX149:resume\n");
    nsx_delay_us(100000);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(output_lifecycle);
    nsx_test_finish();
}
