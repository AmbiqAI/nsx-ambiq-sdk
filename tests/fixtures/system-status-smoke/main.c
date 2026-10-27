#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "unity.h"
#include "result_reporter.h"

void setUp(void) {}
void tearDown(void) {}

static void reject_then_start(void) {
    nsx_system_config_t cfg = {.enable_cache=true, .skip_bsp_init=NSX_TEST_MINIMAL,
                               .debug={.transport=NSX_DEBUG_ITM}};
    uint32_t mask=__get_PRIMASK();
    cfg.perf_mode=(nsx_perf_mode_e)99;
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_system_init(&cfg));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_set_perf_mode(cfg.perf_mode));
    cfg.perf_mode=NSX_PERF_LOW;
    cfg.debug.transport=(nsx_debug_transport_e)99;
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_system_init(&cfg));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_debug_init(&cfg.debug));
    cfg.debug.transport=NSX_DEBUG_ITM;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&cfg));
    TEST_ASSERT_EQUAL_UINT32(mask, __get_PRIMASK());
    /* Check configuration after successful trace setup; result delivery uses RTT. */
    TEST_ASSERT_BITS_HIGH(ITM_TCR_ITMENA_Msk, ITM->TCR);
    TEST_ASSERT_NOT_EQUAL(0, ITM->TER);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(reject_then_start);
    nsx_test_finish();
}
