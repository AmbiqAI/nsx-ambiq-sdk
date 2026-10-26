#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "unity.h"
#include "result_reporter.h"

static uint32_t seed_usb, seed_phy, seed_state;
static const uint32_t usb_mask = PWRCTRL_DEVPWRSTATUS_PWRSTUSB_Msk |
                                 PWRCTRL_DEVPWRSTATUS_PWRSTUSBPHY_Msk;
void __real_am_bsp_low_power_init(void);
void __wrap_am_bsp_low_power_init(void) {
    __real_am_bsp_low_power_init();
    // Establish powered domains after the real BSP baseline, before NSX policy.
    seed_usb = am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_USB);
    seed_phy = am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_USBPHY);
    seed_state = PWRCTRL->DEVPWRSTATUS & usb_mask;
}

void setUp(void) {}
void tearDown(void) {
    uint32_t usb = am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_USB);
    uint32_t phy = am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_USBPHY);
    TEST_ASSERT_EQUAL_UINT32(0, usb);
    TEST_ASSERT_EQUAL_UINT32(0, phy);
}

static void usb_policy(void) {
    nsx_power_config_t cfg = nsx_power_all_on;
    cfg.perf_mode = NSX_POWER_PERF_LOW;
    cfg.need_usb = NSX_TEST_USB_KEEP;
    uint32_t status = nsx_power_configure(&cfg);
    TEST_ASSERT_EQUAL_UINT32(0, seed_usb);
    TEST_ASSERT_EQUAL_UINT32(0, seed_phy);
    TEST_ASSERT_EQUAL_HEX32(usb_mask, seed_state);
    TEST_ASSERT_EQUAL_UINT32(0, status);
    TEST_ASSERT_EQUAL_HEX32(NSX_TEST_USB_KEEP ? usb_mask : 0,
                            PWRCTRL->DEVPWRSTATUS & usb_mask);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(usb_policy);
    nsx_test_finish();
}
