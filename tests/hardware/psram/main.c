#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_psram.h"
#include "nsx_mem.h"
#include "unity.h"
#include "result_reporter.h"
#include <string.h>

#define MAX_BYTES 8195u
#define OFFSET 0x10000u
static uint8_t source[MAX_BYTES + 17u], destination[MAX_BYTES + 17u];
/* Covers the BA driver's 48 MHz scan windows (4 KiB windows, turnaround 6,
 * three Boolean offsets and up to 124 bytes of DQS offset). */
#define SCAN_COVER_BYTES (64u * 1024u)
static uint32_t scan_source[SCAN_COVER_BYTES / sizeof(uint32_t)];
static uint32_t init_xip_mismatches, init_xip_first_bad;
static bool ready;
static volatile uint32_t whole_cache_operations;
uint32_t __real_am_hal_cachectrl_dcache_invalidate(am_hal_cachectrl_range_t *range, bool clean);
uint32_t __wrap_am_hal_cachectrl_dcache_invalidate(am_hal_cachectrl_range_t *range, bool clean) {
    if (range == NULL) ++whole_cache_operations;
    return __real_am_hal_cachectrl_dcache_invalidate(range, clean);
}
#ifdef NSX_TEST_TRANSFER_FLUSH
uint32_t __real_nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length);
uint32_t __wrap_nsx_psram_write(uint32_t offset, const void *buffer, uint32_t length) {
    uint32_t status=__real_nsx_psram_write(offset, buffer, length);
    am_hal_cachectrl_dcache_invalidate(NULL, true);
    return status;
}
#endif
void setUp(void) {}
void tearDown(void) {}

static void initialize(void) {
    nsx_system_config_t system = {.enable_cache=true, .perf_mode=NSX_PERF_LOW,
                                  .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __enable_irq();
    for (unsigned i=0; i<sizeof(scan_source)/sizeof(scan_source[0]); ++i)
        scan_source[i]=0xa6935c17u ^ (i*2654435761u);
    nsx_psram_config_t config;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_default_config(&config));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_init(&config));
    nsx_psram_info_t info;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
    TEST_ASSERT_EQUAL_UINT32(48000000u, info.configured_clock_hz);
    TEST_ASSERT_TRUE(info.size_bytes > OFFSET + sizeof(source));
    /* No XIP read or caller cache maintenance between calibration and DMA. */
    uint32_t maintenance_before=whole_cache_operations;
    TEST_ASSERT_TRUE(maintenance_before > 0u); /* Verify the linker hook is active. */
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(0, scan_source, sizeof(scan_source)));
    TEST_ASSERT_EQUAL_UINT32(maintenance_before, whole_cache_operations);
    const volatile uint32_t *xip=(const volatile uint32_t *)(uintptr_t)info.base_address;
    for (unsigned i=0; i<sizeof(scan_source)/sizeof(scan_source[0]); ++i) {
        if (xip[i] != scan_source[i]) {
            if (!init_xip_mismatches) init_xip_first_bad=i*sizeof(uint32_t);
            ++init_xip_mismatches;
        }
    }
    ready = true;
}

static void calibration_xip(void) {
    TEST_ASSERT_TRUE(ready);
    UnityPrint("init_xip_mismatches="); UnityPrintNumberUnsigned(init_xip_mismatches);
    UnityPrint(" first_bad_offset="); UnityPrintNumberUnsigned(init_xip_first_bad);
    UnityPrint("\n");
    TEST_ASSERT_EQUAL_UINT32(0, init_xip_mismatches);
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

static void caller_xip_sync(void) {
    TEST_ASSERT_TRUE(ready);
    nsx_psram_info_t info;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    uint32_t value=0x11223344u;
    const volatile uint32_t *xip=(const volatile uint32_t *)(uintptr_t)(info.base_address+OFFSET);
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_UINT32(value, *xip); /* Populate a caller-owned clean alias. */
    value=0x55667788u;
    uint32_t maintenance_before=whole_cache_operations;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(OFFSET, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_UINT32(maintenance_before, whole_cache_operations);
    TEST_ASSERT_EQUAL_UINT32(0, nsx_cache_sync_shared_data());
    TEST_ASSERT_EQUAL_UINT32(value, *xip);
}

static void refusals(void) {
    TEST_ASSERT_TRUE(ready);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,
                             nsx_psram_read(OFFSET+1, destination+1, 4));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,
                             nsx_psram_write(OFFSET+1, source+1, 4));
    const unsigned invalid[] = {1, 2, 3, 45, 4095, 4097, MAX_BYTES};
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_read(OFFSET, source, MAX_BYTES+9));
    for (unsigned i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i)
        TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,
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
    RUN_TEST(calibration_xip);
    RUN_TEST(roundtrip);
    RUN_TEST(refusals);
    RUN_TEST(caller_xip_sync);
    nsx_test_finish();
}
