/**
 * @file nsx_system.h
 * @brief NSX-first system initialization for staged Ambiq targets.
 *
 * Provides composable, order-aware startup building blocks so that
 * applications can enable only the peripherals and subsystems they need.
 *
 * This header is the preferred NSX-facing entry point for common system
 * bring-up. AmbiqSuite initialization details remain important implementation
 * context, but callers should normally reach for the NSX helpers first and
 * only drop lower when they need behavior that NSX does not wrap yet.
 *
 * ## Boot Sequence Overview
 *
 * The ARM Cortex-M goes through several stages before reaching main().
 * Details vary by staged R5 SoC (M55 on Apollo5/510/330P):
 *
 * ```
 *  Reset_Handler (startup_gcc.c)
 *    ├─ VTOR, SP, FPU enable
 *    ├─ MSPLIM / PSPLIM (stack guard, M55 only)
 *    ├─ Copy .data → TCM                ← initialized globals
 *    ├─ Copy .shared → SHARED_SRAM      ← NSX_MEM_SRAM data
 *    ├─ Copy .itcm_text → ITCM          ← NSX_MEM_FAST_CODE (AP510)
 *    ├─ Zero .bss in TCM
 *    ├─ Zero .sram_bss in SHARED_SRAM   ← NSX_MEM_SRAM_BSS
 *    ├─ SystemInit()                     ← CMSIS, sets clocks
 *    ├─ __libc_init_array()              ← C++ static constructors
 *    └─ main()
 * ```
 *
 * ## Recommended Initialization Order in main()
 *
 * The `nsx_system_init()` convenience function performs all steps in the
 * recommended order. For fine-grained control, call the individual helpers
 * in this order:
 *
 * ```c
 * // 1. Core runtime (required first — gates all other NSX calls)
 * nsx_core_init(&core_cfg);
 *
 * // 2. Low-power hardware init (required for cache, clocks, SIMOBUCK)
 * //    NOTE: am_bsp_low_power_init() includes a 2-second delay.
 * //    Use nsx_minimal_hw_init() instead if you don't need full BSP setup.
 * nsx_hw_init();               // or nsx_minimal_hw_init()
 *
 * // 3. Cache (requires CPDLP from step 2 — will fail without it)
 * nsx_cache_enable();           // from nsx_mem.h
 *
 * // 4. Performance mode (HP clock)
 * nsx_set_perf_mode(NSX_PERF_HIGH);
 *
 * // 5. Debug output (order matters — see gotchas below)
 * nsx_debug_init(&debug_cfg);
 *
 * // 6. Application-specific peripherals
 * // ... your code ...
 * ```
 *
 * ## Common Gotchas
 *
 * ### I/D Cache silently not enabled (Apollo5 family)
 *   `am_hal_cachectrl_icache_enable()` checks `PWRMODCTL->CPDLPSTATE` and
 *   returns FAIL (silently!) if the cache power domain (RLP) is not active.
 *   This is configured by `am_hal_pwrctrl_low_power_init()` inside
 *   `am_bsp_low_power_init()`. Without it, inference runs 3–4x slower
 *   with no visible error.
 *   → Always call `nsx_hw_init()` or `nsx_minimal_hw_init()` before
 *     `nsx_cache_enable()`. Check the return value.
 *
 * ### SpotManager requires init before profile_set (Apollo5 family)
 *   `am_hal_spotmgr_init()` is called inside `am_hal_pwrctrl_low_power_init()`.
 *   Calling `am_hal_spotmgr_profile_set()` without it is undefined behavior.
 *   SpotManager is available only where the R5 HAL exposes it.
 *
 * ### SWO/ITM printf dependencies (all SoCs with DCU)
 *   Getting SWO printf output on Apollo5-family SoCs requires unlocking the
 *   DCU, which the Secure Bootloader (SBL) locks before transferring to user
 *   code. `nsx_debug_init()` exists so applications do not have to open-code
 *   that sequence.
 *
 *   The specific sequence is:
 *   1. **DCU unlock** — OTP + Crypto powered temporarily, `am_hal_dcu_update()`
 *      enables SWO/DWT/SWD/TRACE bits, then Crypto/OTP are powered down.
 *      This step is shared by all Apollo5 variants and is handled internally.
 *   2. **TPIU / ITM / SWO pin** — Configured by BSP helper on AP330P/510L,
 *      or manually on Apollo510/5B/5A (where `MCUCTRL_DBGCTRL_DBGTPIUCLKSEL_HFRC_96MHz`
 *      is available).
 *   3. **printf backend** — `am_util_stdio_printf_init()` with `am_hal_itm_print`.
 *
 * ### SWO Trace Clock and JLink Frequency
 *   The TPIU generates SWO output at: `baud = trace_clk / (ACPR + 1)`.
 *   JLink SWO viewer overrides ACPR based on what it believes is the trace
 *   clock (passed via `-cpufreq`). If the value is wrong, the baud rate
 *   mismatch causes silent data loss — SWO connects but shows no output.
 *
 *   | SoC         | Trace clock source | Freq     | JLink -cpufreq |
 *   |-------------|--------------------|----------|----------------|
 *   | Apollo510   | HFRC_96MHz         | 96 MHz   | 96000000       |
 *   | Apollo330P  | XTAL_HS            | 48 MHz   | 48000000       |
 *
 *   The `nsx view` CLI command and the CMake `<app>_view` target handle
 *   this automatically via the per-SoC `NSX_SEGGER_CPUFREQ` setting in
 *   `cmake/segger/socs/<soc>.cmake`. When using JLink manually:
 *   ```
 *   JLinkSWOViewerCLExe -device <dev> -cpufreq <trace_freq> -swofreq 1000000 -itmport 0
 *   ```
 *
 * ### am_bsp_low_power_init() disables debug by default
 *   It calls `am_hal_debug_trace_disable()` and powers down the DEBUG
 *   peripheral for minimum power. If you need ITM/SWO, configure debug
 *   AFTER this call.
 *
 * ### C++ Static Constructors
 *   `__libc_init_array()` in the startup code runs C++ global constructors
 *   BEFORE main(). These run with hardware in reset-default state (no
 *   caches, no SIMOBUCK, no clock config). Keep global constructors
 *   lightweight — defer hardware-dependent init to main().
 *   The linker script MUST have `.preinit_array`, `.init_array`, and
 *   `.fini_array` sections or constructors silently won't run.
 *
 * ### Heap/Stack in TCM
 *   Both .stack and .heap are in MCU_TCM and are placed BEFORE .data
 *   and .bss. An oversized HEAP_SIZE (defined before including
 *   startup_gcc.c) consumes TCM budget and can push .data/.bss past
 *   the TCM boundary, causing a link-time overflow error. The heap is
 *   sized in uint32_t words, so HEAP_SIZE=1024 = 4KB.
 *   On staged Apollo510+ targets, stack and heap are separate sections in TCM.
 *
 * @copyright Copyright (c) 2024-2026, Ambiq Micro, Inc.
 */
#ifndef NSX_SYSTEM_H
#define NSX_SYSTEM_H

#include "nsx_core.h"
#include "nsx_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Performance mode
 * =================================================================== */

/** @brief CPU performance / clock speed setting.
 *
 *  | SoC       | LOW         | MEDIUM      | HIGH        |
 *  |-----------|-------------|-------------|-------------|
 *  | Apollo510 | LP  96 MHz  | HP  250 MHz | HP  250 MHz |
 *  | AP330P    | LP  96 MHz  | HP1         | HP2         |
 */
typedef enum {
    NSX_PERF_LOW    = 0,   ///< Low-power mode
    NSX_PERF_MEDIUM = 1,   ///< Medium performance (HP1 on AP330P/510L)
    NSX_PERF_HIGH   = 2,   ///< High performance  (HP2 on AP330P/510L)
} nsx_perf_mode_e;

/* ===================================================================
 * Debug output configuration
 * =================================================================== */

/** @brief Preferred NSX debug-output transport. */
typedef enum {
    NSX_DEBUG_NONE = 0,    ///< No debug output
    NSX_DEBUG_ITM  = 1,    ///< SWO/ITM (requires JLink + SWO pin)
    NSX_DEBUG_UART = 2,    ///< UART (requires BSP UART pins)
} nsx_debug_transport_e;

/**
 * @brief Debug output configuration.
 *
 * Controls ITM/SWO or UART printf output. Handles the staged NSX debug
 * sequence, including any required lower-level enablement, transport setup,
 * and printf backend registration.
 */
typedef struct {
    nsx_debug_transport_e transport;  ///< Which output to enable
} nsx_debug_config_t;

/* ===================================================================
 * System configuration (one-shot convenience)
 * =================================================================== */

/**
 * @brief Complete system initialization configuration.
 *
 * Pass to nsx_system_init() for a single-call setup. Every field
 * has a safe default (zero-init = minimal, low-power, no debug).
 *
 * For fine-grained control, skip this and call individual helpers.
 */
typedef struct {
    nsx_perf_mode_e perf_mode;       ///< CPU performance mode
    bool            enable_cache;    ///< Enable I/D cache on staged R5 targets
    bool            enable_sram;     ///< Reserved — keep shared SRAM powered (not yet wired)

    nsx_debug_config_t debug;        ///< Debug output config

    bool            skip_bsp_init;   ///< Skip am_bsp_low_power_init() (use minimal HW init)
    bool            spot_mgr_profile;///< Enable SpotManager profile (Apollo5 family only)
} nsx_system_config_t;

/* ===================================================================
 * One-call system init
 * =================================================================== */

/**
 * @brief Initialize the system with a single call.
 *
 * Performs all steps in the correct order:
 *   1. nsx_core_init()
 *   2. Hardware init (BSP or minimal)
 *   3. Cache enable (if requested)
 *   4. Performance mode
 *   5. SpotManager profile (if requested)
 *   6. Debug output (if requested)
 *
 * @param cfg  System configuration. Must not be NULL.
 * @return 0 on success, non-zero on failure.
 */
uint32_t nsx_system_init(const nsx_system_config_t *cfg);

/* ===================================================================
 * Individual startup building blocks
 *
 * Call these in order if you need fine-grained control.
 * See "Recommended Initialization Order" in the file header.
 * =================================================================== */

/**
 * @brief Full staged hardware init via BSP.
 *
 * Calls am_bsp_low_power_init() which, depending on SoC:
 *   - (AP510) Waits ~2 seconds, configures SIMOBUCK/CPDLP/clocks/SpotManager
 *
 * On all SoCs this call disables debug/trace for minimum power.
 * Call nsx_debug_init() AFTER this if you need ITM/SWO.
 *
 * @return 0 on success.
 */
uint32_t nsx_hw_init(void);

/**
 * @brief Minimal staged hardware init (no BSP, no 2-second delay).
 *
 * Performs the minimum setup needed for basic operation:
 *   - Apollo5: CPDLP config (cache power domain), SpotManager init, FPU
 *
 * Does NOT configure SIMOBUCK, clock gates, or disable peripherals.
 * On Apollo5, caches are NOT enabled — call nsx_cache_enable() after.
 *
 * Use this when you want fast startup and don't need full low-power
 * optimization.
 *
 * @return 0 on success.
 */
uint32_t nsx_minimal_hw_init(void);

/**
 * @brief Set CPU performance mode.
 *
 * @param mode  Desired performance level.
 * @return 0 on success.
 */
uint32_t nsx_set_perf_mode(nsx_perf_mode_e mode);

/**
 * @brief Initialize NSX-managed debug output (ITM/SWO or UART).
 *
 * On all Apollo5 variants (AP510, AP330P, etc.) the Secure Bootloader locks
 * the DCU before transferring to user code.  This function unlocks SWO via
 * OTP + Crypto power-cycling and am_hal_dcu_update(), then configures TPIU
 * and ITM for printf output.
 *
 * Platform-specific behavior:
 *   - Apollo510/5B/5A: DCU unlock + manual TPIU (HFRC_96MHz, 96 MHz trace
 *     clock) + SWO pin (GPIO 28) + printf backend.
 *   - Apollo330P/510L: DCU unlock + am_bsp_itm_printf_enable() (XTAL_HS,
 *     48 MHz trace clock).
 *
 * @param cfg  Debug configuration. NULL disables debug output.
 * @return 0 on success.
 */
uint32_t nsx_debug_init(const nsx_debug_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* NSX_SYSTEM_H */
