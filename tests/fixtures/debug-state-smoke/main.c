#include "am_mcu_apollo.h"
#include "nsx_system.h"
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B)
#include "nsx_power.h"
#endif
#include "unity.h"
#include "result_reporter.h"

#ifdef NSX_TEST_SWO_HANDSHAKE
volatile uint32_t nsx_debug_capture_ready;
volatile uint32_t nsx_debug_capture_go;
#endif

void setUp(void) {}
void tearDown(void) {}

#ifdef NSX_TEST_SWO_FAILURE
static unsigned preparation_calls;
uint32_t __wrap_am_hal_dcu_swo_enable(void) {
    ++preparation_calls;
    return AM_HAL_STATUS_FAIL;
}

static void preparation_failure(void) {
    nsx_system_config_t cfg = {.enable_cache=true};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&cfg));
    uint32_t domains = PWRCTRL->DEVPWRSTATUS;
    uint32_t primask = __get_PRIMASK();
    nsx_debug_config_t debug = {.transport=NSX_DEBUG_ITM};
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STATUS_FAIL, nsx_debug_init(&debug));
    TEST_ASSERT_EQUAL_UINT32(primask, __get_PRIMASK());
    const uint32_t mask = PWRCTRL_DEVPWRSTATUS_PWRSTOTP_Msk |
                          PWRCTRL_DEVPWRSTATUS_PWRSTCRYPTO_Msk;
    TEST_ASSERT_EQUAL_HEX32(domains & mask, PWRCTRL->DEVPWRSTATUS & mask);
    TEST_ASSERT_EQUAL_UINT32(1, preparation_calls);
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STATUS_FAIL, nsx_debug_init(&debug));
    TEST_ASSERT_EQUAL_UINT32(1, preparation_calls);
}
#else
static void output_lifecycle(void) {
    nsx_system_config_t cfg = {.enable_cache=true, .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&cfg));
#ifdef NSX_TEST_SWO_HANDSHAKE
    nsx_debug_capture_ready = 1;
    while (!nsx_debug_capture_go) __asm volatile("nop");
#endif
    nsx_low_power_printf("NSX149:system\n");
    nsx_itm_printf_enable();
    nsx_debug_printf_enable();
    nsx_low_power_printf("NSX149:repeat\n");
    nsx_delay_us(100000);
    nsx_debug_printf_disable();
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    /* AP3 gates trace access and the TPIU clock; gated ITM reads are not
     * the disable contract. Check the HAL's final clock/trace state. */
    TEST_ASSERT_BITS_LOW(CoreDebug_DEMCR_TRCENA_Msk, CoreDebug->DEMCR);
    TEST_ASSERT_EQUAL_UINT32(0, MCUCTRL->TPIUCTRL);
#else
    TEST_ASSERT_BITS_LOW(ITM_TCR_ITMENA_Msk, ITM->TCR);
#endif
    nsx_low_power_printf("NSX149:SHOULD_NOT_PRINT\n");
    nsx_delay_us(100000);
    nsx_debug_printf_enable();
    TEST_ASSERT_BITS_HIGH(ITM_TCR_ITMENA_Msk, ITM->TCR);
    nsx_low_power_printf("NSX149:resume\n");
    nsx_delay_us(100000);
#if defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B)
    nsx_power_disable_debug();
    TEST_ASSERT_BITS_LOW(ITM_TCR_ITMENA_Msk, ITM->TCR);
    nsx_low_power_printf("NSX149:POWER_SHOULD_NOT_PRINT\n");
    nsx_itm_printf_enable();
    TEST_ASSERT_BITS_HIGH(ITM_TCR_ITMENA_Msk, ITM->TCR);
    nsx_low_power_printf("NSX149:power-resume\n");
    nsx_delay_us(100000);
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_shutdown_peripherals());
    nsx_low_power_printf("NSX149:SHUTDOWN_SHOULD_NOT_PRINT\n");
    nsx_itm_printf_enable();
    nsx_low_power_printf("NSX149:shutdown-resume\n");
    nsx_delay_us(100000);
#endif
}

#endif

int main(void) {
    nsx_test_begin(__FILE__);
#ifdef NSX_TEST_SWO_FAILURE
    RUN_TEST(preparation_failure);
#else
    RUN_TEST(output_lifecycle);
#endif
    nsx_test_finish();
}
