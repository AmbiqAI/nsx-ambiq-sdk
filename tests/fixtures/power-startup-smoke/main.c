#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "nsx_system.h"
#include "unity.h"
#include "result_reporter.h"

volatile uint32_t power_shared_data __attribute__((section(".shared"))) = 0x1234abcd;
volatile uint32_t power_shared_bss __attribute__((section(".sram_bss")));
void setUp(void) {}
void tearDown(void) {}

static void configure_then_reject(void) {
    nsx_power_config_t config=nsx_power_all_on;
    config.small_tcm=false;
    config.need_ssram=true;
    config.perf_mode=NSX_POWER_PERF_LOW;
    volatile uint32_t stack_sentinel=0x98765432;
    TEST_ASSERT_EQUAL_HEX32(0x1234abcd, power_shared_data);
    TEST_ASSERT_EQUAL_UINT32(0, power_shared_bss);
    power_shared_bss=0x5a5ac3c3;
    uint32_t mask=__get_PRIMASK();
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_configure(&config));
    TEST_ASSERT_EQUAL_UINT32(mask, __get_PRIMASK());
    TEST_ASSERT_EQUAL_UINT32(96000000, SystemCoreClock);
    am_hal_pwrctrl_mcu_mode_e mode;
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_mcu_mode_status(&mode));
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER, mode);
    TEST_ASSERT_BITS_HIGH(SCB_CCR_IC_Msk | SCB_CCR_DC_Msk, SCB->CCR);
    TEST_ASSERT_EQUAL_HEX32(0x1234abcd, power_shared_data);
    TEST_ASSERT_EQUAL_HEX32(0x5a5ac3c3, power_shared_bss);
    TEST_ASSERT_EQUAL_HEX32(0x98765432, stack_sentinel);
    nsx_system_config_t system={0};
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_power_configure(&config));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_system_init(&system));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_hw_init());
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_minimal_hw_init());
}
int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(configure_then_reject);
    nsx_test_finish();
}
