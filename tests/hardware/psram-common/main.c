#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_psram.h"
#include "unity.h"
#include "result_reporter.h"
#include <string.h>

#define OFFSET 0x10000u
#define MAX_BYTES 8192u
/* Common v1 path: word-sized offsets, buffers and lengths on both SDKs. */
static uint32_t source[(MAX_BYTES + 32u) / 4u] __attribute__((aligned(32)));
static uint32_t destination[(MAX_BYTES + 32u) / 4u] __attribute__((aligned(32)));
static nsx_psram_config_t config;
static nsx_psram_info_t info;
static bool ready;
void setUp(void) {}
void tearDown(void) {}

static void initialize(void) {
    nsx_system_config_t system = {.enable_cache=true, .perf_mode=NSX_PERF_LOW,
                                  .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_UNINITIALIZED, info.state);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_NOT_INITIALIZED,
                             nsx_psram_read(OFFSET, destination, 4));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_NOT_INITIALIZED,
                             nsx_psram_write(OFFSET, source, 4));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_default_config(&config));
    config.api = &nsx_psram_V1_0_0;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_init(&config));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
    TEST_ASSERT_EQUAL_UINT32(48000000u, info.configured_clock_hz);
    TEST_ASSERT_TRUE(info.size_bytes > OFFSET + sizeof(source));
    TEST_ASSERT_TRUE(info.capabilities & NSX_PSRAM_CAP_SYNC_TRANSFER);
    ready = true;
}

static void aligned_roundtrip(void) {
    TEST_ASSERT_TRUE(ready);
    const uint32_t sizes[] = {4u, 32u, 4092u, 4096u, 4100u, MAX_BYTES};
    for (unsigned s=0; s<sizeof(sizes)/sizeof(sizes[0]); ++s) {
        uint32_t n=sizes[s];
        memset(source, 0xcc, sizeof(source));
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET, source, n+8));
        for (uint32_t i=0; i<n/4; ++i) source[i]=0xa6935c17u ^ (i*2654435761u+s);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET+4, source, n));
        memset(destination, 0x5a, sizeof(destination));
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET+4, destination+1, n));
        TEST_ASSERT_EQUAL_MEMORY(source, destination+1, n);
        TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a, destination[0]);
        for (unsigned i=n/4+1; i<sizeof(destination)/sizeof(destination[0]); ++i)
            TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a, destination[i]);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, destination, n+8));
        TEST_ASSERT_EQUAL_HEX32(0xcccccccc, destination[0]);
        TEST_ASSERT_EQUAL_MEMORY(source, destination+1, n);
        TEST_ASSERT_EQUAL_HEX32(0xcccccccc, destination[n/4+1]);
    }
}

static void common_refusals(void) {
    TEST_ASSERT_TRUE(ready);
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_HANDLE, nsx_psram_get_info(NULL));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_psram_read(OFFSET, NULL, 4));
    TEST_ASSERT_EQUAL_UINT32(NSX_STATUS_INVALID_CONFIG, nsx_psram_write(OFFSET, NULL, 4));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_OUT_OF_RANGE,
                             nsx_psram_read(info.size_bytes, destination, 4));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_OUT_OF_RANGE,
                             nsx_psram_write(info.size_bytes-4, source, 8));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(UINT32_MAX, NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(UINT32_MAX, NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALREADY_INITIALIZED, nsx_psram_init(&config));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
    uint32_t value=0x3851ab93u, readback=0u;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET, &value, 4));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, &readback, 4));
    TEST_ASSERT_EQUAL_UINT32(value, readback);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(initialize);
    RUN_TEST(aligned_roundtrip);
    RUN_TEST(common_refusals);
    nsx_test_finish();
}
