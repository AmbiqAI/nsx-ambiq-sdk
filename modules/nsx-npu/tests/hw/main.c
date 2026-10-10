/**
 * @file main.c
 * @brief On-FPGA hardware test for the Atomiq110 STIMER timebase behind
 * nsx-ethos-u-driver's bounded Ethos-U85 inference wait
 * (modules/nsx-npu/src/atomiq110/nsx_npu_timebase.c).
 *
 * This test links the real, in-tree nsx::npu / nsx::core CMake targets,
 * exercising the actual module boundary the SDK ships. See
 * tests/hw/CMakeLists.txt for how the module graph is assembled into this
 * executable.
 *
 * Checks performed, in order, over the ITM/SWO debug transport:
 *
 *  0. nsx_npu_init() succeeds and nsx_npu_timebase_status() reports ARMED.
 *  1. nsx_ethos_u_ticks_per_ms() == 0 before init, == 1000 once armed.
 *  2. nsx_ethos_u_ticks() never returns 0 while armed, and is monotonic
 *     non-decreasing across repeated calls.
 *  3. Elapsed-time accuracy: the virtual clock's delta across a
 *     known-duration busy-wait is within tolerance of that duration.
 *  4. A real Ethos-U85 inference (a canned, Vela-compiled ResNet model)
 *     completes with ETHOSU_JOB_RESULT_OK and matches the golden output.
 *  5. nsx_npu_deinit() followed by a fresh nsx_npu_init() re-arms cleanly.
 */

#include "am_mcu_apollo.h"

#include "nsx_core.h"
#include "nsx_system.h"
#include "nsx_npu.h"
#include "nsx_ethos_u.h"

#include "resnet_v1_8_32_tfs_int8_buffers.h"
#include "resnet_v1_8_32_tfs_int8_meta.h"
#include "resnet_v1_8_32_tfs_int8_cmd_data.h"

static uint32_t g_ui32Pass = 0;
static uint32_t g_ui32Fail = 0;

#define CHECK(desc, cond)                                                     \
    do {                                                                      \
        if (cond) {                                                          \
            nsx_printf("[PASS] %s\n", desc);                                 \
            g_ui32Pass++;                                                    \
        } else {                                                             \
            nsx_printf("[FAIL] %s\n", desc);                                 \
            g_ui32Fail++;                                                    \
        }                                                                     \
    } while (0)

static const char *timebase_status_str(nsx_npu_timebase_status_e s)
{
    switch (s)
    {
        case NSX_NPU_TIMEBASE_ARMED:              return "ARMED";
        case NSX_NPU_TIMEBASE_NOT_INITIALIZED:    return "NOT_INITIALIZED";
        case NSX_NPU_TIMEBASE_UNSUPPORTED_CLOCK:  return "UNSUPPORTED_CLOCK";
        case NSX_NPU_TIMEBASE_COUNTER_STOPPED:    return "COUNTER_STOPPED";
        default:                                  return "UNKNOWN";
    }
}

// Golden output (10 int8 values) from resnet_v1_8_32_tfs_int8_golden_output.txt
static const int8_t resnet_golden_output[RESNET_V1_8_32_TFS_INT8_OUTPUT0_SIZE] = {
    -71, -118, -105, -97, -97, -118, -97, -115, -97, -111
};

static uint32_t run_resnet_inference(void)
{
    // Reuse the nsx_npu-owned driver handle (already ethosu_init()'d and
    // IRQ-wired by nsx_npu_init()) instead of creating a second local
    // driver instance, so the NPU interrupt (routed by nsx_npu.c) dispatches
    // to the same instance whose ethosu_wait() the timebase under test
    // is polled from.
    struct ethosu_driver *drv = nsx_npu_driver();
    if (drv == NULL)
    {
        return (uint32_t)ETHOSU_JOB_RESULT_ERROR;
    }

    uint64_t base_addr[ETHOSU_MAX_REGIONS] = {0};
    size_t   base_size[ETHOSU_MAX_REGIONS] = {0};

    for (int r = 0; r < ETHOSU_MAX_REGIONS; ++r)
    {
        uint8_t *p = get_region_base_ptr(r);
        size_t   s = get_region_size(r);
        if ((p != NULL) && (s != 0U))
        {
            base_addr[r] = (uint64_t)(uintptr_t)p;
            base_size[r] = s;
        }
    }

    return (uint32_t)ethosu_invoke(drv,
                                    resnet_v1_8_32_tfs_int8_cmd_data,
                                    (int)resnet_v1_8_32_tfs_int8_cmd_size,
                                    base_addr, base_size, ETHOSU_MAX_REGIONS);
}

static int resnet_output_matches_golden(void)
{
    uint8_t *region_1 = get_region_base_ptr(RESNET_V1_8_32_TFS_INT8_OUTPUT0_REGION);
    for (int i = 0; i < RESNET_V1_8_32_TFS_INT8_OUTPUT0_SIZE; i++)
    {
        if ((uint8_t)resnet_golden_output[i] != region_1[i])
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    // Single-call board bring-up: core init, low-power HW init, I/D cache,
    // and ITM/SWO-routed nsx_printf().
    nsx_system_config_t sys_cfg = {
        .perf_mode = NSX_PERF_LOW,
        .enable_cache = true,
        .debug = { .transport = NSX_DEBUG_ITM },
    };
    uint32_t ui32SysStatus = nsx_system_init(&sys_cfg);
    if (ui32SysStatus != NSX_STATUS_SUCCESS)
    {
        // Nothing to print to yet if UART bring-up itself failed.
        while (1) {}
    }

    nsx_printf("\nnsx-npu timebase hardware test (nsx-ambiq-sdk in-tree)\n");
    nsx_printf("==============================================================\n\n");

    // Check 0/1a: before nsx_npu_init(), the weak default hooks are in
    // effect -- both must report the "no timebase" sentinel.
    CHECK("ticks_per_ms() == 0 before nsx_npu_init()", nsx_ethos_u_ticks_per_ms() == 0);

    // Bring up the NPU + timebase. FPGA/pre-silicon: tolerate the
    // power-ack handshake and skip performance-mode programming.
    nsx_npu_config_t npu_cfg = {
        .perf_mode = NSX_NPU_PERF_HIGH_PERFORMANCE,
        .skip_perf_mode = true,
        .tolerate_power_ack = true,
    };

    uint32_t ui32InitStatus = nsx_npu_init(&npu_cfg);
    CHECK("nsx_npu_init() == NSX_STATUS_SUCCESS", ui32InitStatus == NSX_STATUS_SUCCESS);

    nsx_npu_timebase_status_e eStatus = nsx_npu_timebase_status();
    nsx_printf("       nsx_npu_timebase_status() = %s\n", timebase_status_str(eStatus));
    CHECK("timebase status == ARMED", eStatus == NSX_NPU_TIMEBASE_ARMED);

    // Check 1: virtual clock rate is the fixed 1000 ticks/ms once armed.
    uint32_t ui32TicksPerMs = nsx_ethos_u_ticks_per_ms();
    nsx_printf("       nsx_ethos_u_ticks_per_ms() = %u\n", ui32TicksPerMs);
    CHECK("ticks_per_ms() == 1000 once armed", ui32TicksPerMs == 1000U);

    // Check 2: ticks() never 0 while armed, and monotonic non-decreasing.
    uint64_t ui64T0 = nsx_ethos_u_ticks();
    uint64_t ui64T1 = nsx_ethos_u_ticks();
    uint64_t ui64T2 = nsx_ethos_u_ticks();
    CHECK("ticks() never 0 while armed", (ui64T0 != 0) && (ui64T1 != 0) && (ui64T2 != 0));
    CHECK("ticks() monotonic non-decreasing", (ui64T1 >= ui64T0) && (ui64T2 >= ui64T1));

    // Check 3: elapsed-time accuracy against a known busy-wait, +/-20% to
    // absorb print overhead and CLKMGR-derived rate rounding. Diagnostic
    // instrumentation prints CLKMGR's own HFRC report and an independent
    // raw STIMER counter delta, to distinguish a silicon-nominal fallback
    // from a genuinely wrong CLKMGR Hz value.
    uint32_t ui32ClkMgrHfrcHz = 0U;
    uint32_t ui32ClkMgrStatus = am_hal_clkmgr_clock_config_get(
        AM_HAL_CLKMGR_CLK_ID_HFRC, &ui32ClkMgrHfrcHz, NULL);
    nsx_printf("       CLKMGR HFRC query: status=%u hz=%u (500MHz fallback %s)\n",
               ui32ClkMgrStatus, ui32ClkMgrHfrcHz,
               (ui32ClkMgrStatus != AM_HAL_STATUS_SUCCESS || ui32ClkMgrHfrcHz == 0U)
                   ? "WOULD trigger" : "not needed");

    uint32_t ui32ClkSelInUse = _FLD2VAL(STIMER_STCFG_CLKSEL, STIMER->STCFG);
    nsx_printf("       STIMER STCFG.CLKSEL in use = %u\n", ui32ClkSelInUse);

    const uint32_t ui32DelayUs = 50000U; // 50 ms
    uint64_t ui64Before = nsx_ethos_u_ticks();
    uint32_t ui32RawBefore = am_hal_stimer_counter_get();
    nsx_delay_us(ui32DelayUs);
    uint64_t ui64After = nsx_ethos_u_ticks();
    uint32_t ui32RawAfter = am_hal_stimer_counter_get();
    uint64_t ui64ElapsedUs = ui64After - ui64Before;

    uint32_t ui32RawDelta = ui32RawAfter - ui32RawBefore;
    uint64_t ui64ObservedTapHz =
        ((uint64_t)ui32RawDelta * 1000000ULL) / (uint64_t)ui32DelayUs;
    nsx_printf("       raw STIMER delta over %u us: %u counts => observed tap rate ~%llu Hz\n",
               ui32DelayUs, ui32RawDelta, (unsigned long long)ui64ObservedTapHz);
    nsx_printf("       elapsed virtual us across a %u us delay: %llu\n",
               ui32DelayUs, (unsigned long long)ui64ElapsedUs);
    CHECK("elapsed time within +/-20% of the real delay",
          (ui64ElapsedUs > (uint64_t)(ui32DelayUs * 80U / 100U)) &&
          (ui64ElapsedUs < (uint64_t)(ui32DelayUs * 120U / 100U)));

    // Check 4: a real inference still completes correctly with the
    // timebase armed and being polled underneath ethosu_wait().
    uint32_t ui32InferStatus = run_resnet_inference();
    CHECK("ResNet inference == ETHOSU_JOB_RESULT_OK", ui32InferStatus == ETHOSU_JOB_RESULT_OK);
    CHECK("ResNet output matches golden data", resnet_output_matches_golden());

    // Check 5: deinit/reinit re-arms cleanly (disarm-on-deinit fix).
    uint32_t ui32DeinitStatus = nsx_npu_deinit();
    CHECK("nsx_npu_deinit() == NSX_STATUS_SUCCESS", ui32DeinitStatus == NSX_STATUS_SUCCESS);
    CHECK("ticks_per_ms() == 0 after deinit", nsx_ethos_u_ticks_per_ms() == 0);

    uint32_t ui32ReinitStatus = nsx_npu_init(&npu_cfg);
    nsx_npu_timebase_status_e eReStatus = nsx_npu_timebase_status();
    CHECK("nsx_npu_init() re-succeeds after deinit", ui32ReinitStatus == NSX_STATUS_SUCCESS);
    CHECK("timebase re-ARMED after deinit/reinit", eReStatus == NSX_NPU_TIMEBASE_ARMED);

    // Summary.
    nsx_printf("\n==============================================================\n");
    nsx_printf("RESULT: %u passed, %u failed\n", g_ui32Pass, g_ui32Fail);
    nsx_printf(g_ui32Fail == 0 ? "OVERALL: PASS\n" : "OVERALL: FAIL\n");

    while (1) {}
}
