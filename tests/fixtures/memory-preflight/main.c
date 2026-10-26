#include <stdint.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "unity.h"
#include "result_reporter.h"

extern unsigned char __HeapBase, __HeapLimit;
volatile uint32_t shared_initialized __attribute__((section(".shared"))) = 0x1234abcd;
volatile uint32_t shared_zeroed __attribute__((section(".sram_bss")));
static volatile uint32_t bsp_calls;
static uint8_t *allocation;
static const size_t allocation_size = 160 * 1024;
void __real_am_bsp_low_power_init(void);
void __wrap_am_bsp_low_power_init(void) {
    ++bsp_calls;
    __real_am_bsp_low_power_init();
}

void setUp(void) {}
void tearDown(void) { free(allocation); allocation = NULL; }

static void reject_reductions_then_start(void) {
    TEST_ASSERT_EQUAL_HEX32(0x10060000, (uintptr_t)&__HeapLimit);
    TEST_ASSERT_EQUAL_HEX32(0x1234abcd, shared_initialized);
    TEST_ASSERT_EQUAL_UINT32(0, shared_zeroed);
    shared_zeroed = 0x5a5ac3c3;
    allocation = malloc(allocation_size);
    TEST_ASSERT_NOT_NULL(allocation);
    TEST_ASSERT_TRUE((uintptr_t)allocation >= (uintptr_t)&__HeapBase);
    TEST_ASSERT_TRUE((uintptr_t)allocation + allocation_size <= (uintptr_t)&__HeapLimit);
    TEST_ASSERT_TRUE((uintptr_t)allocation + allocation_size > 0x10020000);
    allocation[0] = 0x12;
    allocation[allocation_size-1] = 0x34;
    volatile uint32_t stack_sentinel = 0x98765432;
    const uint32_t before[] = {PWRCTRL->MEMPWREN, PWRCTRL->MEMPWRSTATUS,
        PWRCTRL->MEMRETCFG, PWRCTRL->SSRAMPWREN, PWRCTRL->SSRAMPWRST};
    const uint32_t mask = __get_PRIMASK();
    for (unsigned small=0; small<2; ++small) {
        for (unsigned ssram=0; ssram<2; ++ssram) {
            if (!small && ssram) continue;
            nsx_power_config_t cfg = nsx_power_all_on;
            cfg.small_tcm = small;
            cfg.need_ssram = ssram;
            TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_power_configure(&cfg));
            TEST_ASSERT_EQUAL_UINT32(0, bsp_calls);
            TEST_ASSERT_EQUAL_UINT32(mask, __get_PRIMASK());
            const uint32_t after[] = {PWRCTRL->MEMPWREN, PWRCTRL->MEMPWRSTATUS,
                PWRCTRL->MEMRETCFG, PWRCTRL->SSRAMPWREN, PWRCTRL->SSRAMPWRST};
            TEST_ASSERT_EQUAL_HEX32_ARRAY(before, after, sizeof(before)/sizeof(before[0]));
            TEST_ASSERT_EQUAL_HEX32(0x1234abcd, shared_initialized);
            TEST_ASSERT_EQUAL_HEX32(0x5a5ac3c3, shared_zeroed);
            TEST_ASSERT_EQUAL_HEX32(0x98765432, stack_sentinel);
            TEST_ASSERT_EQUAL_HEX8(0x12, allocation[0]);
            TEST_ASSERT_EQUAL_HEX8(0x34, allocation[allocation_size-1]);
        }
    }
    nsx_power_config_t safe = nsx_power_all_on;
    safe.perf_mode = NSX_POWER_PERF_LOW;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_power_configure(&safe));
    TEST_ASSERT_EQUAL_UINT32(1, bsp_calls);
    TEST_ASSERT_EQUAL_HEX32(0x1234abcd, shared_initialized);
    TEST_ASSERT_EQUAL_HEX32(0x5a5ac3c3, shared_zeroed);
    TEST_ASSERT_EQUAL_HEX8(0x12, allocation[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, allocation[allocation_size-1]);
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_STARTUP_REJECTED, nsx_power_configure(&safe));
    TEST_ASSERT_EQUAL_UINT32(1, bsp_calls);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(reject_reductions_then_start);
    nsx_test_finish();
}
