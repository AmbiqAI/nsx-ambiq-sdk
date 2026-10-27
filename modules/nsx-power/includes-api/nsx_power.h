/**
 * @file nsx_power.h
 * @author Ambiq
 * @brief NSX power-management helpers for supported AmbiqSuite Apollo targets.
 * @version 0.1
 * @date 2022-09-02
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 * \addtogroup nsx-power
 *  @{
 */

//*****************************************************************************
//
// Copyright (c) 2021, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This interface is staged for supported AmbiqSuite-backed NSX Apollo targets.
//
//*****************************************************************************
#ifndef NSX_POWER_H
    #define NSX_POWER_H

    #include "nsx_core.h"

    #ifdef __cplusplus
extern "C" {
    #endif

    #define NSX_POWER_V0_0_1                                                                        \
        { .major = 0, .minor = 0, .revision = 1 }
    #define NSX_POWER_V1_0_0                                                                        \
        { .major = 1, .minor = 0, .revision = 0 }

    #define NSX_POWER_OLDEST_SUPPORTED_VERSION NSX_POWER_V0_0_1
    #define NSX_POWER_CURRENT_VERSION NSX_POWER_V1_0_0
    #define NSX_POWER_API_ID 0xCA0007

extern const nsx_core_api_t nsx_power_V0_0_1;
extern const nsx_core_api_t nsx_power_V1_0_0;
extern const nsx_core_api_t nsx_power_oldest_supported_version;
extern const nsx_core_api_t nsx_power_current_version;

/// CPU performance / clock-speed mode.
typedef enum {
    NSX_POWER_PERF_NOT_SET = -1,
    NSX_POWER_PERF_LOW     = 0, ///< Family low-power mode.
    NSX_POWER_PERF_HIGH    = 1, ///< Apollo510: configured HFRC2; Apollo330P/510L: HP1 192 MHz.
    NSX_POWER_PERF_MAX     = 2  ///< Apollo330P/510L: configured HP2 PLL rate; otherwise HIGH.
} nsx_power_perf_mode_t;

/// Power configuration for the common NSX-managed power states.
///
/// On AP4P/L, supported need_* peripheral flags skip optional shutdown;
/// they do not initialize drivers or undo the mandatory BSP baseline.
/// AP3/3P do not translate these peripheral flags. See the module support table.
/// This is the preferred NSX-facing interface for common power flows.
/// Lower-level AmbiqSuite power APIs remain available underneath when an
/// application needs finer control than the staged NSX helpers expose.
typedef struct {
    const nsx_core_api_t *api;       ///< API version
    nsx_power_perf_mode_t perf_mode;       ///< CPU clock mode
    bool need_audadc;               ///< Skip optional audio ADC shutdown where supported
    bool need_ssram;                ///< Shared SRAM requirement; AP4P/L requires true, AP3/3P unmanaged
    bool need_crypto;               ///< Skip optional crypto shutdown
    bool need_ble;                  ///< Skip optional BLE GPIO shutdown; BSP reset still applies
    bool need_usb;                  ///< Skip optional USB/PHY shutdown on AP4P
    bool need_iom;                  ///< Skip optional IOM shutdown
    bool need_uart;                 ///< Skip optional UART0/3 shutdown on AP4
    bool small_tcm;                 ///< Reduced TCM request; AP4P/L rejects true, AP3/3P unmanaged
    bool need_tempco;               ///< Enable Apollo4 TempCo where supported
    bool need_itm;                  ///< Keep ITM/SWO debug output
    bool need_xtal;                 ///< Skip optional XTAL deep-sleep powerdown policy
    bool spotmgr_collapse;          ///< SpotManager STM state collapse (Apollo5 family)
} nsx_power_config_t;

extern const nsx_power_config_t nsx_power_all_on;   ///< Development preset; crypto and TempCo remain false
extern const nsx_power_config_t nsx_power_minimal;   ///< Request supported shutdowns; check memory layout before use
extern const nsx_power_config_t nsx_power_audio;     ///< Skip supported audio ADC shutdown; drivers still required

/// Two-bit GPIO state exported for external current/energy tools.
typedef enum {
    NSX_POWER_MONITOR_STATE_IDLE = 0,
    NSX_POWER_MONITOR_STATE_DATA_COLLECTION = 1,
    NSX_POWER_MONITOR_STATE_FEATURE_EXTRACTION = 2,
    NSX_POWER_MONITOR_STATE_INFERENCE = 3,
} nsx_power_monitor_state_t;

/// Output format for staged power-profile dumps.
typedef enum {
    NSX_POWER_PROFILE_FORMAT_JSON = 0,
    NSX_POWER_PROFILE_FORMAT_CSV = 1,
} nsx_power_profile_format_t;

/**
 * @brief Apply an NSX power configuration.
 *
 * Configures the staged NSX power knobs described by
 * `nsx_power_config_t`. The fields in this structure are intended to cover
 * the common application-facing power cases without forcing apps to manage
 * the lower-level SoC power sequence directly.
 *
 * On Apollo3/3P and Apollo4, invalid performance modes and unsupported TempCo
 * requests are rejected before hardware setup. Core initialization is not a
 * prerequisite. The first failed status-returning configuration operation is
 * returned to the application; this path does not enter the NSX fail loop.
 * Callers must check the result before continuing startup.
 *
 * On AP3/3P and AP4P/L, startup ownership is shared with nsx_system_init(),
 * nsx_hw_init() and nsx_minimal_hw_init(). Choose one per reset, before drivers.
 * A repeated/re-entrant valid request returns NSX_STATUS_STARTUP_REJECTED,
 * including after a reported hardware failure. Earlier writes are not rolled
 * back. Reset before retrying; private helper recovery does not clear ownership.
 * Preflight validation failures do not claim ownership and can be corrected.
 * BSP-internal operations and TempCo resource cleanup are separate concerns.
 *
 * AP4P/L rejects small_tcm=true or need_ssram=false with INVALID_CONFIG before
 * startup ownership or hardware effects, even when API validation is disabled.
 * No supported reduced-memory layout/ownership contract exists yet. Copy the
 * minimal/audio preset and set need_ssram=true; keep small_tcm=false. AP3/3P
 * leave these two fields unmanaged. Other families retain their own contracts.
 *
 * @param nsx_power_config_t Desired power configuration.
 * @return uint32_t success/failure.
 */
extern uint32_t nsx_power_configure(const nsx_power_config_t *);

/**
 * @brief Initialize the external power-monitor GPIO state output.
 *
 * Configures the two monitor GPIOs for the current SoC family and sets the
 * exported state to idle. This helper is intended for external tools such as
 * Joulescope that decode application phases from GPIO transitions.
 *
 * @return `NSX_STATUS_SUCCESS` on success, otherwise an NSX status code.
 */
extern uint32_t nsx_power_monitor_init(void);

/**
 * @brief Update the external power-monitor GPIO state.
 *
 * Has no effect until `nsx_power_monitor_init()` succeeds.
 *
 * @param state New two-bit state to present on the monitor GPIOs.
 */
extern void nsx_power_monitor_set_state(nsx_power_monitor_state_t state);

/**
 * @brief Emit a staged power-profile dump for the current SoC.
 *
 * Apollo4 and Apollo5 families provide a register snapshot dump that is meant
 * for low-power analysis and debug capture. The exact register set is kept
 * intentionally focused on the NSX-owned power and clock domains rather than
 * mirroring the full legacy `nsx-utils` bundle verbatim.
 *
 * @param snapshot_index User-provided snapshot tag included in the output.
 * @param format Output format.
 * @return `NSX_STATUS_SUCCESS` on success, otherwise an NSX status code.
 */
extern uint32_t nsx_power_profile_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format);

/**
 * @brief Enter deep sleep using the staged NSX system sequence.
 *
 * This is a convenience helper for the common sleep path. Applications that
 * need a more specialized suspend/resume flow may still drop to lower-level
 * APIs or explicit helper sequencing.
 */
extern void nsx_power_deep_sleep(void);

/**
 * @brief Set CPU performance mode.
 *
 * Apollo3/3P maps LOW to normal mode and HIGH/MAX to burst mode; Apollo4
 * maps LOW to LP and HIGH/MAX to HP. Both reject NOT_SET and unknown values
 * without a HAL call and propagate HAL errors. AP3 leaves SystemCoreClock unchanged;
 * AP4 silicon publishes 96/192 MHz after hardware ACK/mode verification. AP4
 * FPGA retains its existing publication behavior.
 * Apollo3/3P initializes burst support on the first valid request (including
 * LOW), caches only successful initialization with available burst support,
 * and retries initialization after failure. The caller owns AP3 clock publication
 * and dependent timing updates on all families.
 *
 * On Apollo510/Apollo510B and Apollo330P/510L, success refreshes the CMSIS
 * SystemCoreClock value after hardware ACK/mode verification. NOT_SET retains
 * its LOW compatibility mapping on these families; other unknown values fail
 * before hardware/query effects. Apollo510 HP uses configured HFRC2; Apollo330P/510L
 * HP2 uses configured PLLVCO. Apollo510 pending HFRC2 adjustment still reports
 * nominal free-running 250 MHz. Active 196.608 MHz audio adjustment rejects HP
 * on AP510/B. Failed selection retains prior publication, not guaranteed hardware
 * state; callers must handle errors. FPGA builds
 * use scaled HAL mode constants. The application owns SysTick reloads and other
 * cached timing conversions; update them after a successful switch. Direct
 * HAL clock changes are outside this helper's synchronization contract. If
 * peripheral initialization changes the CPU source, reapply the desired mode
 * afterward, then refresh timing settings.
 *
 * @param mode  Desired performance mode from `nsx_power_perf_mode_t`.
 * @return uint32_t status.
 */
uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode);

/* ===================================================================
 * Power-down helpers
 *
 * Building blocks for low-power operation.  Each does one thing with
 * the correct register sequence.
 *
 * General-purpose staged helpers (safe to call from any app):
 *   nsx_power_shutdown_peripherals() — disable all peripheral domains
 *   nsx_power_minimize_memory()      — smallest TCM, no SSRAM
 *   nsx_power_tristate_gpios()       — float unused pins
 *   nsx_power_disable_debug()        — disable SWO/ITM + debug domain
 *   nsx_power_disable_caches()       — invalidate + disable I/D cache
 *
 * NVM shutdown (irreversible — power measurement only):
 *   nsx_power_disable_nvm()          — request MRAM power-off
 *   NSX_POWER_DRAIN_NVM()            — finalize NVM shutdown
 *
 * Recommended full sequence for minimum active current:
 *   1. nsx_power_disable_debug()
 *   2. nsx_power_shutdown_peripherals()
 *   3. nsx_power_minimize_memory()
 *   4. nsx_power_tristate_gpios(keep_pins, n)
 *   5. nsx_power_set_performance_mode()        — select clock LAST
 *   6. (from ITCM) nsx_power_disable_nvm() + NSX_POWER_DRAIN_NVM()
 *   7. (from ITCM) nsx_power_disable_caches() — if no MRAM calls
 * =================================================================== */

/**
 * @brief Shut down the staged non-core peripheral domains.
 *
 * Disables all device power domains (USB, audio, crypto, IOM, etc.),
 * XTAL oscillator, voltage comparator, and all 16 hardware timers.
 * Releases NSX-managed debug output first; a reported release error stops
 * shutdown. The HAL all-peripheral gate also disables trace. Memory config
 * and GPIO are unchanged — use the dedicated helpers for those.
 *
 * Safe to call from any app that no longer needs peripheral I/O.
 *
 * This helper is intentionally generic. Board-device policy and
 * application-specific sequencing should stay outside `nsx-power`.
 *
 * @return 0 on success.
 */
uint32_t nsx_power_shutdown_peripherals(void);

/**
 * @brief Configure the staged minimal-memory profile.
 *
 * Sets 32 KB ITCM + 128 KB DTCM, single NVM bank, no shared SRAM,
 * and enables MRAM low-power read mode.  Suitable for small
 * ITCM-resident workloads with data in DTCM only.
 *
 * Any code or data outside these regions will be inaccessible
 * after this call.  Ensure your stack, heap, and working buffers
 * fit within 128 KB DTCM.
 *
 * @return 0 on success.
 */
uint32_t nsx_power_minimize_memory(void);

/**
 * @brief Request NVM (MRAM) power-off.
 *
 * Clears PWRENNVM for both NVM banks and issues barrier.
 * The NVM controller will not finalize power-down until an MRAM bus
 * transaction completes — call NSX_POWER_DRAIN_NVM() afterward.
 *
 * @warning After NSX_POWER_DRAIN_NVM(), MRAM is inaccessible.
 *          The caller must already be executing from ITCM/TCM.
 */
void nsx_power_disable_nvm(void);

/**
 * @brief Force one MRAM read to drain the NVM pipeline.
 *
 * After nsx_power_disable_nvm(), the NVM controller waits for
 * outstanding MRAM fetches before powering down.  If the CPU never
 * touches MRAM (e.g. all code inlined into ITCM), NVM stays on.
 * This macro forces one volatile read to complete the shutdown.
 *
 * Must be called from ITCM-resident code.  After this macro,
 * MRAM is powered off — no further MRAM access is possible.
 */
    #define NSX_POWER_DRAIN_NVM() do { \
        volatile uint32_t _nvm_drain = *(volatile uint32_t *)0x00430000; \
        (void)_nvm_drain; \
    } while (0)

/**
 * @brief Invalidate and disable I-cache and D-cache.
 *
 * For ITCM-only execution, caches consume power with no benefit
 * since ITCM/DTCM are zero-wait-state.
 *
 * @note If any code still calls functions in MRAM (e.g. libc
 *       memset), keep the I-cache enabled — those calls are
 *       served from the warm cache even after NVM shutdown.
 */
void nsx_power_disable_caches(void);

/**
 * @brief Disable SWO/ITM debug output and the debug peripheral.
 *
 * Releases any NSX-managed ITM transport before the platform debug gate.
 * UART transport ownership is unchanged. A reported ITM teardown failure
 * suppresses low-power output and is retained by nsx_debug_init until reset.
 * Call before entering measurement — debug logic draws current.
 */
void nsx_power_disable_debug(void);

/**
 * @brief Tristate all GPIO pads except specified pins.
 *
 * Suspend any transport whose pads will be tristated before calling this.
 *
 * Unconfigured GPIOs can leak current through internal pull
 * resistors.  This sets every pad to disabled (high-impedance)
 * except those listed in @p keep_pins.
 *
 * Typical keep list: power-monitor GPIOs, wake-up pins, or
 * any pin that must remain driven during measurement or sleep.
 *
 * @param keep_pins  Array of GPIO numbers to leave configured.
 * @param n_keep     Number of entries in keep_pins.
 */
void nsx_power_tristate_gpios(const uint32_t *keep_pins, uint32_t n_keep);

    #ifdef __cplusplus
}
    #endif
#endif // NSX_POWER_H
/** @}*/
