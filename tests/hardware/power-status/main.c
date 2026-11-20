#include "am_mcu_apollo.h"
#include "nsx_power_status.h"
#include "result_reporter.h"
#include "unity.h"

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
#define IOM0_REQUEST PWRCTRL_DEVPWREN_PWRIOM0_Msk
#define IOM0_DOMAIN PWRCTRL_DEVPWRSTATUS_HCPB_Msk
#define SUPPLY_CONTROL (PWRCTRL->SUPPLYSRC)
#else
#define IOM0_REQUEST PWRCTRL_DEVPWREN_PWRENIOM0_Msk
#define IOM0_DOMAIN PWRCTRL_DEVPWRSTATUS_PWRSTIOM0_Msk
#define SUPPLY_CONTROL (PWRCTRL->VRCTRL)
#endif

static bool restore_iom0;
static uint32_t initial_requests;

void setUp(void) {}

static bool wait_for_domain(bool powered) {
    for (unsigned poll = 0; poll < 100000u; ++poll) {
        if (((PWRCTRL->DEVPWRSTATUS & IOM0_DOMAIN) != 0u) == powered) {
            return true;
        }
    }
    return false;
}

void tearDown(void) {
    // Unity also calls tearDown after a failed assertion or a HAL timeout.
    if (restore_iom0) {
        if (PWRCTRL->DEVPWREN & IOM0_REQUEST) {
            TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM0));
        }
        TEST_ASSERT_TRUE_MESSAGE(wait_for_domain(false), "IOM0 domain did not return to idle");
        TEST_ASSERT_EQUAL_HEX32(initial_requests, PWRCTRL->DEVPWREN);
    }
}

static void check_snapshot(bool powered) {
    TEST_ASSERT_TRUE_MESSAGE(wait_for_domain(powered), "IOM0 power domain did not settle");
    const uint32_t requests = PWRCTRL->DEVPWREN;
    const uint32_t memory = PWRCTRL->MEMPWREN;
    const uint32_t supply_control = SUPPLY_CONTROL;
    const uint32_t irq_mask = __get_PRIMASK();
    nsx_power_status_t status;
    TEST_ASSERT_EQUAL_UINT32(NSX_POWER_STATUS_OK, nsx_power_status_read(&status));
    TEST_ASSERT_EQUAL_HEX32(requests, PWRCTRL->DEVPWREN);
    TEST_ASSERT_EQUAL_HEX32(memory, PWRCTRL->MEMPWREN);
    TEST_ASSERT_EQUAL_HEX32(supply_control, SUPPLY_CONTROL);
    TEST_ASSERT_EQUAL_UINT32(irq_mask, __get_PRIMASK());
    TEST_ASSERT_BITS_HIGH(NSX_POWER_STATUS_VALID_DEVICE, status.valid);
    TEST_ASSERT_EQUAL_HEX32(powered ? IOM0_DOMAIN : 0u, status.device & IOM0_DOMAIN);

    UnityPrint("device="); UnityPrintNumberHex(status.device, 8);
    UnityPrint(" valid="); UnityPrintNumberHex(status.valid, 8);
    UnityPrint(" supply="); UnityPrintNumberHex(status.supply, 8);
    UnityPrint(" regulator="); UnityPrintNumberHex(status.regulator, 8);
    UnityPrint(" cmsis_core_hz="); UnityPrintNumberUnsigned(SystemCoreClock);
    UNITY_OUTPUT_CHAR('\n');
}

static void iom0_power_transition(void) {
    // This standalone image has no IOM clients. Decline to take over an
    // existing request, including another user's share of an Apollo3 domain.
    initial_requests = PWRCTRL->DEVPWREN;
    TEST_ASSERT_BITS_LOW_MESSAGE(IOM0_REQUEST, initial_requests, "IOM0 is already requested");
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    TEST_ASSERT_BITS_LOW_MESSAGE(PWRCTRL_DEVPWREN_PWRIOM1_Msk | PWRCTRL_DEVPWREN_PWRIOM2_Msk,
                                 initial_requests, "Another IOM holds the shared HCPB domain");
#endif
    TEST_ASSERT_TRUE_MESSAGE(wait_for_domain(false), "IOM0 domain must be idle for this fixture");
    restore_iom0 = true;
    check_snapshot(false);
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_IOM0));
    check_snapshot(true);
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM0));
    check_snapshot(false);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(iom0_power_transition);
    nsx_test_finish();
}
