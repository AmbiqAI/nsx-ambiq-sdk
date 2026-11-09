#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_psram.h"
#include "unity.h"
#include "result_reporter.h"
#include <string.h>

#define MAX_BYTES 8195u
#define OFFSET 0x10000u
static uint8_t source[MAX_BYTES + 17u], destination[MAX_BYTES + 17u];
static bool ready;
void setUp(void) {}
void tearDown(void) {}

static void initialize(void) {
    nsx_system_config_t system = {.enable_cache=true, .perf_mode=NSX_PERF_LOW,
                                  .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __enable_irq();
    nsx_psram_config_t config;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_default_config(&config));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_init(&config));
    nsx_psram_info_t info;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
    TEST_ASSERT_EQUAL_UINT32(48000000u, info.configured_clock_hz);
    TEST_ASSERT_TRUE(info.size_bytes > OFFSET + sizeof(source));
    ready = true;
}

static void roundtrip(void) {
    TEST_ASSERT_TRUE(ready);
    const uint32_t sizes[] = {1u, 2u, 3u, 4u, 45u, 4095u, 4096u, 4097u, MAX_BYTES};
    for (unsigned s=0; s<sizeof(sizes)/sizeof(sizes[0]); ++s) {
        uint32_t n=sizes[s], write_bytes=(n+3u)&~3u;
        memset(source, 0xcc, sizeof(source));
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET, source+1, write_bytes+8));
        /* Dirty unused DMA-buffer bytes independently of the PSRAM guards. */
        memset(source, 0xa5, sizeof(source));
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET+0x8000u, source+1, 4096));
        for (uint32_t i=0; i<write_bytes; ++i) source[i+1]=(uint8_t)(i*17u+s+9u);
        uint32_t start=DWT->CYCCNT;
        uint32_t status=nsx_psram_write(OFFSET+4, source+1, write_bytes);
        uint32_t write_cycles=DWT->CYCCNT-start;
        TEST_ASSERT_EQUAL_UINT32(0, status);
        memset(destination, 0x5a, sizeof(destination));
        start=DWT->CYCCNT;
        status=nsx_psram_read(OFFSET+4, destination+1, n);
        uint32_t read_cycles=DWT->CYCCNT-start;
        TEST_ASSERT_EQUAL_UINT32(0, status);
        TEST_ASSERT_EQUAL_MEMORY(source+1, destination+1, n);
        TEST_ASSERT_EQUAL_HEX8(0x5a, destination[0]);
        for (uint32_t i=n+1; i<sizeof(destination); ++i)
            TEST_ASSERT_EQUAL_HEX8(0x5a, destination[i]);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, destination+1, write_bytes+8));
        for (unsigned i=0; i<4; ++i) {
            TEST_ASSERT_EQUAL_HEX8(0xcc, destination[i+1]);
            TEST_ASSERT_EQUAL_HEX8(0xcc, destination[write_bytes+5+i]);
        }
        start=DWT->CYCCNT;
        memcpy(destination+1, source+1, n);
        __DMB();
        uint32_t copy_cycles=DWT->CYCCNT-start;
        TEST_ASSERT_EQUAL_MEMORY(source+1, destination+1, n);
        UnityPrint("read_bytes="); UnityPrintNumberUnsigned(n);
        UnityPrint(" write_bytes="); UnityPrintNumberUnsigned(write_bytes);
        UnityPrint(" cpu_hz="); UnityPrintNumberUnsigned(SystemCoreClock);
        UnityPrint(" write_cycles="); UnityPrintNumberUnsigned(write_cycles);
        UnityPrint(" read_cycles="); UnityPrintNumberUnsigned(read_cycles);
        UnityPrint(" cpu_copy_cycles="); UnityPrintNumberUnsigned(copy_cycles);
        UnityPrint("\n");
    }
}

static void refusals(void) {
    TEST_ASSERT_TRUE(ready);
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STATUS_INVALID_OPERATION,
                             nsx_psram_read(OFFSET+1, destination+1, 4));
    TEST_ASSERT_EQUAL_UINT32(AM_HAL_STATUS_INVALID_OPERATION,
                             nsx_psram_write(OFFSET+1, source+1, 4));
    const unsigned invalid[] = {1, 2, 3, 45, 4095, 4097, MAX_BYTES};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, source, MAX_BYTES+9));
    for (unsigned i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i)
        TEST_ASSERT_EQUAL_UINT32(AM_HAL_STATUS_INVALID_OPERATION,
                                 nsx_psram_write(OFFSET, destination, invalid[i]));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, destination, MAX_BYTES+9));
    TEST_ASSERT_EQUAL_MEMORY(source, destination, MAX_BYTES+9);
    nsx_psram_info_t info;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(initialize);
    RUN_TEST(roundtrip);
    RUN_TEST(refusals);
    nsx_test_finish();
}
