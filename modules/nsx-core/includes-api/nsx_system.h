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
 * Startup and standalone performance/debug helpers reject unknown enum values
 * before hardware changes. nsx_debug_init(NULL) and NSX_DEBUG_NONE remain
 * successful no-ops. Status-returning setup failures stop subsequent setup;
 * already completed hardware steps are not rolled back. DCU setup restores
 * its interrupt mask and attempts cleanup of temporarily powered domains.
 * Void or nonreturning vendor helpers cannot report failures through this API.
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
 * Choose either nsx_system_init() or a staged sequence, before application
 * drivers, DMA or a scheduler. Check every returned status. A staged sequence
 * is nsx_core_init(), one of nsx_hw_init()/nsx_minimal_hw_init(), optional
 * nsx_cache_enable(), nsx_debug_init(), then nsx_set_perf_mode(). Do not call
 * nsx_system_init() after a staged hardware-init entry point. The C runtime
 * and static constructors have already run; nsx_core_init() only establishes
 * NSX lifecycle/API state.
 *
 * Minimal startup is an advanced board/boot-dependent path, not a faster
 * equivalent of the full BSP baseline. It does not establish all regulator
 * and clock prerequisites for high-performance operation. Debug setup must
 * precede the Apollo5-class performance switch.
 *
 * ## Common Gotchas
 *
 * ### I/D Cache silently not enabled (Apollo5 family)
 *   `am_hal_cachectrl_icache_enable()` checks `PWRMODCTL->CPDLPSTATE` and
 *   returns FAIL if the cache power domain (RLP) is not active.
 *   This is configured by `am_hal_pwrctrl_low_power_init()` inside
 *   `am_bsp_low_power_init()`. Without it, inference runs 3–4x slower
 *   if the caller ignores the returned error.
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
 *   2. **TPIU / ITM / SWO pin** — Configured by checked HAL calls on AP330P/510L,
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
 *   | Apollo330P  | HFRC_48MHz         | 48 MHz   | 48000000       |
 *
 *   The `nsx view` CLI command and the CMake `<app>_view` target handle
 *   this automatically via the per-SoC `NSX_SEGGER_CPUFREQ` setting in
 *   `cmake/socs/facts/<soc>.cmake`. When using JLink manually:
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
 *   BEFORE main(). Hardware state depends on the bootloader and SystemInit(),
 *   not the later application startup request. Keep global constructors
 *   lightweight — defer hardware-dependent init to main().
 *   The linker script MUST have `.preinit_array`, `.init_array`, and
 *   `.fini_array` sections or constructors silently won't run.
 *
 * ### Heap and stack placement (GCC/ATfE)
 *   .stack is a static section placed before .data and .bss. .heap is
 *   placed after .bss and takes the rest of that RAM region (TCM on
 *   Apollo4/Apollo5, main SRAM on Apollo2/Apollo3); nsx-core's _sbrk()
 *   bounds malloc to [__HeapBase, __HeapLimit). Each linker script asserts
 *   that more than 1 KiB of heap remains.
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
 *  | Family      | LOW       | MEDIUM | HIGH |
 *  |-------------|-----------|--------|------|
 *  | Apollo3/3P  | Normal 24/48 MHz | Burst 96 MHz | Burst 96 MHz |
 *  | Apollo4P/L  | LP        | HP     | HP   |
 *  | Apollo510/B | LP        | HP     | HP   |
 *  | AP330P/510L | LP        | HP1    | HP2  |
 *
 * These are legacy system-selector meanings, not the nsx-power enum layout.
 * AP3/3P shares burst control with nsx_power_set_performance_mode().
 * On AP3/3P and AP4 silicon and Apollo510/B/AP330P/510L, successful selection publishes
 * SystemCoreClock after verifying hardware mode. CPU rates depend on the
 * configured source. The caller owns SysTick/RTOS reloads and cached conversions.
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
    NSX_DEBUG_NONE = 0,    ///< Request no debug setup; not a shutdown operation
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
 * Pass to nsx_system_init() for a single-call setup.
 * Zero-initialization selects full BSP initialization, LOW performance and
 * no explicit cache/debug/profile request. It does not select minimal startup
 * or guarantee that the BSP leaves caches/debug domains disabled.
 *
 * For fine-grained control, skip this and call individual helpers.
 */
typedef struct {
    nsx_perf_mode_e perf_mode;       ///< CPU performance mode
    bool            enable_cache;    ///< Request cache enable after hardware init; false does not disable cache
    bool            enable_sram;     ///< Reserved, ignored; does not change memory power or layout

    nsx_debug_config_t debug;        ///< Debug output config

    bool            skip_bsp_init;   ///< Skip am_bsp_low_power_init() (use minimal HW init)
    bool            spot_mgr_profile;///< Profile request on Apollo510/5A/5B; ignored by AP3/AP4/AP330P/510L
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
 *   4. Debug output (if requested)
 *   5. Performance mode
 *   6. SpotManager profile (if requested)
 *
 * AP3/3P, AP4P/L, AP510/B/5A/5B and AP330P/510L share one startup guard
 * across this function,
 * nsx_hw_init(), nsx_minimal_hw_init() and nsx_power_configure(). Choose one
 * per reset, from foreground before drivers. Valid duplicate/re-entrant calls
 * return NSX_STATUS_STARTUP_REJECTED, including after a reported setup failure.
 * AP2 and Atomiq110 currently do not enforce this shared ownership guard.
 * Preflight errors are retryable. skip_bsp_init selects minimal initialization
 * and still consumes ownership. Direct HAL/BSP calls bypass this guard.
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
 * Calls the board's am_bsp_low_power_init(); its side effects and delays are
 * board-specific. The BSP has a void interface, so its internal failures
 * cannot be propagated. This operation may reset peripheral/debug state.
 * Active NSX-managed debug output is released first; that failure stops setup.
 * Configure debug after the BSP baseline if output is required.
 *
 * @return 0 on completion, non-zero on NSX ownership/debug-release failure.
 */
uint32_t nsx_hw_init(void);

/**
 * @brief Minimal staged hardware init without the full BSP baseline.
 *
 * AP3/3P and AP4P/L configure/enable cache and FPU. Apollo5-class backends
 * configure CPDLP, initialize SpotManager and enable FPU; their cache-enable
 * operation remains separate. Therefore enable_cache=false in system init
 * is not a cache-disable request. This path does not establish the complete
 * board regulator, clock or peripheral baseline needed by every application.
 *
 * @return 0 on success, non-zero on ownership or reported setup failure.
 */
uint32_t nsx_minimal_hw_init(void);

/**
 * @brief Apply the legacy family performance mapping shown above.
 *
 * Publishes SystemCoreClock on AP3/3P and AP4 silicon and Apollo510/B/AP330P/510L only
 * after successful selection and hardware ACK/mode verification. On failure
 * the previous publication is retained; hardware rollback is not guaranteed.
 * Caller owns hardware prerequisites, serialized access and SysTick/RTOS timing.
 * AP510/B HP is rejected while audio-adjusted 196.608 MHz HFRC2 is active.
 * AP3 LOW preserves the normal divider (24/48 MHz) without initializing burst.
 * AP3 burst requires CORESEL=0, an allowed SKU, enabled BLE buck and (AP3P) settled
 * core supply selection; minimal startup alone does not prepare these supplies.
 * HAL owns patch-dependent workarounds. AP3 FPGA selection is unsupported.
 * AP4 FPGA, AP2 and AT110 retain their existing publication behavior.
 * Unknown modes fail before hardware effects.
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
 *   - Apollo330P/510L: DCU preparation + checked HAL ITM/TPIU setup
 *     (HFRC_48MHz, 48 MHz trace clock).
 *
 * Calls must be serialized by the application. Only NSX-managed transports
 * participate in this bookkeeping; do not mix raw BSP/HAL setup with it.
 * Re-enabling the active transport is a no-op. Switching first disables the
 * old transport, then enables the new one. Only successful setup permits
 * nsx_low_power_printf output. On a reported setup/disable error, output is
 * suppressed and subsequent setup returns that error until reset; partial
 * hardware changes are not rolled back. Legacy void BSP helpers cannot
 * expose failures. Underlying vendor HAL waits may still be unbounded.
 * Full BSP initialization releases the active NSX transport
 * first and stops if that release reports an error.
 *
 * Apollo5 ITM setup/resume retains the existing clock precondition: configure
 * it while running on HFRC, before switching to a SYSPLL high-performance
 * source. This helper does not change CPU clock policy on resume.
 *
 * @param cfg Debug configuration. NULL and NONE are successful no-ops.
 * @return 0 on success.
 */
uint32_t nsx_debug_init(const nsx_debug_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* NSX_SYSTEM_H */
