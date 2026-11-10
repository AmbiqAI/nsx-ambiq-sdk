#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_psram.h"
#include "unity.h"
#include "result_reporter.h"
#include <string.h>

#define PIECE_BYTES 4096u
#define PIECES 16u
#define COPY_BYTES (PIECE_BYTES * PIECES)
#define WORDS (PIECE_BYTES / sizeof(uint32_t))
#define SOURCE_OFFSET 0x40000u
#define TRIALS 16u
#define WARMUPS 2u

static uint32_t expected[COPY_BYTES / sizeof(uint32_t)];
static uint32_t storage[(PIECES + 2u) * WORDS] __attribute__((aligned(PIECE_BYTES)));
AM_SHARED_RW static uint32_t shared_storage[(PIECES + 2u) * WORDS]
    __attribute__((aligned(PIECE_BYTES)));
static uint32_t *active_storage = storage;
#define DESTINATION (active_storage + WORDS)
static nsx_psram_info_t info;
static nsx_psram_client_t client;
static void *mspi_handle;
static bool initialized;
static volatile bool measuring;
static volatile uint32_t callbacks, callback_errors, interrupts, interrupt_bits, hal_calls;
static volatile uint32_t first_callback, last_callback;

uint32_t __real_am_hal_mspi_nonblocking_transfer(void *, void *, am_hal_mspi_trans_e,
                                                am_hal_mspi_callback_t, void *);
uint32_t __wrap_am_hal_mspi_nonblocking_transfer(void *handle, void *transfer,
                                               am_hal_mspi_trans_e mode,
                                               am_hal_mspi_callback_t callback, void *ctx) {
    mspi_handle = handle;
    if (measuring) ++hal_calls;
    return __real_am_hal_mspi_nonblocking_transfer(handle, transfer, mode, callback, ctx);
}
uint32_t __real_am_hal_mspi_interrupt_service(void *, uint32_t);
uint32_t __wrap_am_hal_mspi_interrupt_service(void *handle, uint32_t status) {
    if (measuring) {
        ++interrupts;
        interrupt_bits |= status;
    }
    return __real_am_hal_mspi_interrupt_service(handle, status);
}

void setUp(void) {}
void tearDown(void) {}

static void complete(void *ctx, uint32_t status) {
    (void)ctx;
    uint32_t now = DWT->CYCCNT;
    if (callbacks == 0u) first_callback = now;
    last_callback = now;
    if (status != 0u) ++callback_errors;
    __DMB();
    ++callbacks;
}

static void number(uint32_t value) {
    UnityPrintNumberUnsigned(value);
}
static void field(const char *name, uint32_t value) {
    UnityPrint(name);
    number(value);
}
static void line_end(void) { UNITY_OUTPUT_CHAR('\n'); }

static void initialize(void) {
    nsx_system_config_t system = {
        .enable_cache = true,
        .perf_mode = NSX_TEST_CPU_HIGH ? NSX_PERF_HIGH : NSX_PERF_LOW,
        .debug = {.transport = NSX_DEBUG_ITM},
    };
    TEST_ASSERT_EQUAL_UINT32(0, nsx_system_init(&system));
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_CPU_HIGH ? 250000000u : 96000000u, SystemCoreClock);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t before = DWT->CYCCNT;
    __DSB();
    TEST_ASSERT_NOT_EQUAL(before, DWT->CYCCNT);
    nsx_psram_config_t config;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_default_config(&config));
    config.clock_hz = NSX_TEST_PSRAM_CLOCK;
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_init(&config));
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_get_info(&info));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATE_READY, info.state);
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_PSRAM_CLOCK, info.configured_clock_hz);
    TEST_ASSERT_EQUAL_UINT32(PIECES, info.async_depth);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_TIMING_VALID, info.timing_status);
    TEST_ASSERT_TRUE(info.xip_enabled);
    TEST_ASSERT_TRUE((uintptr_t)storage >= DTCM_BASEADDR);
    TEST_ASSERT_TRUE((uintptr_t)storage + sizeof(storage) <= DTCM_BASEADDR + DTCM_MAX_SIZE);
    TEST_ASSERT_TRUE((uintptr_t)shared_storage >= SSRAM_BASEADDR);
    TEST_ASSERT_TRUE((uintptr_t)shared_storage + sizeof(shared_storage) <= SSRAM_BASEADDR + SSRAM_MAX_SIZE);
    TEST_ASSERT_BITS_HIGH(1u, PWRCTRL->SSRAMPWRST);
    for (unsigned i = 0; i < COPY_BYTES / sizeof(uint32_t); ++i)
        expected[i] = 0x713a9c55u ^ (i * 2654435761u);
    TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_write(SOURCE_OFFSET, expected, sizeof(expected)));
    TEST_ASSERT_NOT_NULL(mspi_handle);
    initialized = true;
    field("CONFIG cpu=", SystemCoreClock);
    field(" psram=", info.configured_clock_hz);
    field(" rxdqs=", info.rxdqs_delay);
    field(" part=", MCUCTRL->CHIPPN);
    field(" revision=", MCUCTRL->CHIPREV);
    field(" ioclock=", CLKGEN->MSPIIOCLKCTRL);
    field(" mspicfg=", MSPI0->MSPICFG);
    field(" devcfg=", MSPI0->DEV0CFG);
    field(" boundary=", MSPI0->DEV0BOUNDARY);
    field(" burst=", MSPI0->DMABCOUNT);
    field(" threshold=", MSPI0->DMATHRESH);
    field(" poll_nops=", 1024u);
    field(" ssram_power=", PWRCTRL->SSRAMPWRST);
    line_end();
}

static void wait_for(unsigned target, uint32_t start) {
    while (callbacks < target) {
        if ((uint32_t)(DWT->CYCCNT - start) > SystemCoreClock / 5u) {
            /* This aborts the only measurement test; main publishes failure
             * and idles without reusing any DMA-owned persistent storage. */
            measuring = false;
            TEST_FAIL_MESSAGE("DMA deadline; storage remains owned until reset");
        }
        /* Keep DWT running while avoiding a tight stream of CPU loads from
         * DTCM competing with the DMA destination. Callback times are separate. */
        for (unsigned spin = 0; spin < 1024u; ++spin) __NOP();
    }
    __DMB();
}

static void submit(bool raw, unsigned index, uint32_t length) {
    uint32_t offset = SOURCE_OFFSET + index * PIECE_BYTES;
    uint32_t *destination = DESTINATION + index * WORDS;
    uint32_t status;
    if (raw) {
        /* Same HAL/CQ path, legitimate captured handle, identical destination
         * rules. No public ownership/power queries or sleeping during this row. */
        am_hal_mspi_dma_transfer_t transfer = {
            .ui8Priority = 1,
            .eDirection = AM_HAL_MSPI_RX,
            .ui32TransferCount = length,
            .ui32DeviceAddress = offset,
            .ui32SRAMAddress = (uint32_t)(uintptr_t)destination,
        };
        status = am_hal_mspi_nonblocking_transfer(mspi_handle, &transfer,
                                                 AM_HAL_MSPI_TRANS_DMA, complete, NULL);
    } else {
        status = nsx_psram_read_async(&client, offset, destination, length, complete, NULL);
    }
    if (status != 0u) {
        measuring = false;
        TEST_ASSERT_EQUAL_UINT32(0, status);
    }
}

static void hardware_idle(void) {
    am_hal_mspi_status_t hardware = {0};
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_mspi_status_get(mspi_handle, &hardware));
    TEST_ASSERT_FALSE(hardware.bTIP);
    TEST_ASSERT_EQUAL_UINT32(0, hardware.ui32NumCQEntries);
}

static void measure_row(const char *name, bool raw, bool serial, bool xip, unsigned pieces, uint32_t length) {
    bool shared = active_storage == shared_storage;
    TEST_ASSERT_TRUE(!shared || raw);
    uint32_t best = UINT32_MAX, worst = 0, sum = 0, submit_sum = 0;
    uint32_t irq_sum = 0, call_sum = 0, cb_sum = 0, bits = 0;
    uint32_t first_sum = 0, last_sum = 0;
    for (unsigned trial = 0; trial < TRIALS + WARMUPS; ++trial) {
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_pending());
        hardware_idle();
        memset(active_storage, 0xa5, sizeof(storage));
        am_hal_cachectrl_range_t destination_range = {
            .ui32StartAddr = (uint32_t)(uintptr_t)active_storage,
            .ui32Size = sizeof(storage),
        };
        if (shared) {
            TEST_ASSERT_EQUAL_UINT32(0, am_hal_cachectrl_dcache_clean(&destination_range));
            TEST_ASSERT_EQUAL_UINT32(0, am_hal_cachectrl_dcache_invalidate(&destination_range, false));
            __DSB();
        }
        client = (nsx_psram_client_t){0};
        callbacks = callback_errors = interrupts = interrupt_bits = hal_calls = 0;
        first_callback = last_callback = 0;
        /* Source aliases were never CPU-written. Invalidate their clean cache
         * lines before every method; timing begins after the maintenance. */
        am_hal_cachectrl_range_t range = {
            .ui32StartAddr = info.base_address + SOURCE_OFFSET,
            .ui32Size = COPY_BYTES,
        };
        TEST_ASSERT_EQUAL_UINT32(0, am_hal_cachectrl_dcache_invalidate(&range, false));
        __DSB();
        uint32_t submitted = 0;
        measuring = true;
        uint32_t start = DWT->CYCCNT;
        if (xip) {
            memcpy(DESTINATION, (const void *)(uintptr_t)(info.base_address + SOURCE_OFFSET), COPY_BYTES);
        } else {
            for (unsigned i = 0; i < pieces; ++i) {
                uint32_t a = DWT->CYCCNT;
                submit(raw, i, length);
                submitted += (uint32_t)(DWT->CYCCNT - a);
                if (serial) wait_for(i + 1u, start);
            }
            wait_for(pieces, start);
        }
        __DSB();
        uint32_t elapsed = DWT->CYCCNT - start;
        measuring = false;
        hardware_idle();
        if (shared) {
            TEST_ASSERT_EQUAL_UINT32(0, am_hal_cachectrl_dcache_invalidate(&destination_range, false));
            __DSB();
        }
        TEST_ASSERT_EQUAL_UINT32(0, callback_errors);
        TEST_ASSERT_EQUAL_UINT32(xip ? 0u : pieces, callbacks);
        TEST_ASSERT_EQUAL_UINT32(xip ? 0u : pieces, hal_calls);
        TEST_ASSERT_EQUAL_UINT32(0, nsx_psram_pending());
        TEST_ASSERT_EQUAL_HEX32(0xa5a5a5a5u, DESTINATION[-1]);
        TEST_ASSERT_EQUAL_HEX32(0xa5a5a5a5u, DESTINATION[COPY_BYTES / sizeof(uint32_t)]);
        if (pieces < PIECES)
            TEST_ASSERT_EQUAL_HEX32(0xa5a5a5a5u, DESTINATION[pieces * WORDS]);
        hardware_idle();
        for (unsigned i = 0; i < pieces; ++i) {
            TEST_ASSERT_EQUAL_MEMORY(expected + i * WORDS, DESTINATION + i * WORDS, length);
            if (length < PIECE_BYTES)
                TEST_ASSERT_EQUAL_HEX32(0xa5a5a5a5u, DESTINATION[i * WORDS + length / sizeof(uint32_t)]);
        }
        if (trial < WARMUPS) continue;
        if (elapsed < best) best = elapsed;
        if (elapsed > worst) worst = elapsed;
        sum += elapsed;
        submit_sum += submitted;
        irq_sum += interrupts;
        call_sum += hal_calls;
        cb_sum += callbacks;
        bits |= interrupt_bits;
        if (!xip) {
            first_sum += (uint32_t)(first_callback - start);
            last_sum += (uint32_t)(last_callback - start);
        }
    }
    UnityPrint("PERF "); UnityPrint(name);
    field(" bytes=", pieces * length);
    field(" piece=", length);
    field(" destination=", (uint32_t)(uintptr_t)DESTINATION);
    field(" apb_continuous=", MSPI0->MSPICFG_b.APBCLK);
    field(" trials=", TRIALS);
    field(" warmups=", WARMUPS);
    field(" cycles=", sum / TRIALS);
    field(" min=", best);
    field(" max=", worst);
    field(" submit=", submit_sum / TRIALS);
    field(" first_cb=", first_sum / TRIALS);
    field(" last_cb=", last_sum / TRIALS);
    field(" calls=", call_sum);
    field(" callbacks=", cb_sum);
    field(" irqs=", irq_sum);
    field(" bits=", bits);
    line_end();
}

static void throughput(void) {
    TEST_ASSERT_TRUE(initialized);
    measure_row("public_queue16", false, false, false, PIECES, PIECE_BYTES);
    measure_row("raw_queue16", true, false, false, PIECES, PIECE_BYTES);
    measure_row("public_serial", false, true, false, PIECES, PIECE_BYTES);
    measure_row("raw_serial", true, true, false, PIECES, PIECE_BYTES);
    measure_row("public_single", false, false, false, 1u, PIECE_BYTES);
    measure_row("raw_single", true, false, false, 1u, PIECE_BYTES);
    measure_row("public_1k", false, false, false, PIECES, 1024u);
    measure_row("raw_1k", true, false, false, PIECES, 1024u);
    measure_row("public_64", false, false, false, PIECES, 64u);
    measure_row("raw_64", true, false, false, PIECES, 64u);
    measure_row("cold_xip_memcpy", false, false, true, PIECES, PIECE_BYTES);
    active_storage = shared_storage;
    measure_row("raw_ssram_queue16", true, false, false, PIECES, PIECE_BYTES);
    active_storage = storage;
    hardware_idle();
    uint32_t previous_apb = MSPI0->MSPICFG_b.APBCLK;
    uint32_t continuous_apb = 1u;
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_mspi_control(mspi_handle, AM_HAL_MSPI_REQ_APBCLK, &continuous_apb));
    __DSB();
    TEST_ASSERT_EQUAL_UINT32(1u, MSPI0->MSPICFG_b.APBCLK);
    measure_row("public_apb_on", false, false, false, PIECES, PIECE_BYTES);
    measure_row("raw_apb_on", true, false, false, PIECES, PIECE_BYTES);
    active_storage = shared_storage;
    measure_row("raw_ssram_apb_on", true, false, false, PIECES, PIECE_BYTES);
    active_storage = storage;
    hardware_idle();
    TEST_ASSERT_EQUAL_UINT32(0, am_hal_mspi_control(mspi_handle, AM_HAL_MSPI_REQ_APBCLK, &previous_apb));
    __DSB();
    TEST_ASSERT_EQUAL_UINT32(previous_apb, MSPI0->MSPICFG_b.APBCLK);
}

int main(void) {
    nsx_test_begin("psram-throughput");
    RUN_TEST(initialize);
    RUN_TEST(throughput);
    nsx_test_finish();
}
