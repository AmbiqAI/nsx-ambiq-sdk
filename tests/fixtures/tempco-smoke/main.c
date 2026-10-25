#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "nsx_timer.h"
#include "unity.h"
#include "result_reporter.h"

/* Internal helper lifecycle: this does not qualify temperature sampling/trims. */
extern uint32_t nsx_power_tempco_enable(void);
extern uint32_t nsx_power_tempco_disable(void);
extern nsx_timer_config_t *nsx_timer_config[];
static uint32_t saved_primask;

void setUp(void) { saved_primask = __get_PRIMASK(); __disable_irq(); }
void tearDown(void) {
    uint32_t status = nsx_power_tempco_disable();
    __set_PRIMASK(saved_primask);
    TEST_ASSERT_EQUAL_UINT32(0, status);
}
static void setup_release_retry(void) {
    for (unsigned i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_UINT32(0, nsx_power_tempco_enable());
        TEST_ASSERT_NOT_NULL(nsx_timer_config[NSX_TIMER_TEMPCO]);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_power_tempco_enable());
        TEST_ASSERT_EQUAL_UINT32(0, nsx_power_tempco_disable());
        TEST_ASSERT_NULL(nsx_timer_config[NSX_TIMER_TEMPCO]);
        TEST_ASSERT_EQUAL_UINT32(0, NVIC_GetEnableIRQ(TIMER4_IRQn));
        bool powered = true;
        TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_periph_enabled(AM_HAL_PWRCTRL_PERIPH_ADC, &powered));
        TEST_ASSERT_FALSE(powered);
        void *handle = NULL;
        TEST_ASSERT_EQUAL_UINT32(0, am_hal_adc_initialize(0, &handle));
        TEST_ASSERT_NOT_NULL(handle);
        TEST_ASSERT_EQUAL_UINT32(0, am_hal_adc_deinitialize(handle));
    }
}
int main(void) {
    am_bsp_low_power_init();
    nsx_test_begin(__FILE__);
    RUN_TEST(setup_release_retry);
    nsx_test_finish();
}
