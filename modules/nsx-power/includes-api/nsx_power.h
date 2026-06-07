/**
 * @file nsx_power.h
 * @author Ambiq
 * @brief NSX power-management helpers for AmbiqSuite R5 targets.
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
// This interface is staged for AmbiqSuite R5-backed NSX targets.
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
    NSX_POWER_PERF_LOW     = 0, ///< LP 96 MHz (HFRC)
    NSX_POWER_PERF_HIGH    = 1, ///< HP 250 MHz (HFRC2, Apollo5 family)
    NSX_POWER_PERF_MAX     = 2  ///< HP2 (Apollo510L only, otherwise same as HIGH)
} nsx_power_perf_mode_t;

/// Power configuration for the common NSX-managed power states.
///
/// This is the preferred NSX-facing interface for common power flows.
/// Lower-level AmbiqSuite power APIs remain available underneath when an
/// application needs finer control than the staged NSX helpers expose.
typedef struct {
    const nsx_core_api_t *api;       ///< API version
    nsx_power_perf_mode_t perf_mode;       ///< CPU clock mode
    bool need_audadc;               ///< Keep audio ADC powered
    bool need_ssram;                ///< Keep shared SRAM powered
    bool need_crypto;               ///< Keep crypto block powered
    bool need_ble;                  ///< Keep BLE powered
    bool need_usb;                  ///< Keep USB powered
    bool need_iom;                  ///< Keep IOM interfaces powered
    bool need_uart;                 ///< Keep alternative UART powered
    bool small_tcm;                 ///< Use 32K ITCM + 128K DTCM (vs full)
    bool need_tempco;               ///< Enable temperature compensation
    bool need_itm;                  ///< Keep ITM/SWO debug output
    bool need_xtal;                 ///< Keep XTAL oscillator
    bool spotmgr_collapse;          ///< SpotManager STM state collapse (Apollo5 family)
} nsx_power_config_t;

extern const nsx_power_config_t nsx_power_all_on;   ///< Everything enabled — development/debug
extern const nsx_power_config_t nsx_power_minimal;   ///< Minimal peripherals — good starting point
extern const nsx_power_config_t nsx_power_audio;     ///< Audio ADC on, most else off

/**
 * @brief Apply an NSX power configuration.
 *
 * Configures the staged NSX power knobs described by
 * `nsx_power_config_t`. The fields in this structure are intended to cover
 * the common application-facing power cases without forcing apps to manage
 * the lower-level SoC power sequence directly.
 *
 * @param nsx_power_config_t Desired power configuration.
 * @return uint32_t success/failure.
 */
extern uint32_t nsx_power_configure(const nsx_power_config_t *);

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
 * Does NOT touch memory config, GPIO, or debug — call the dedicated
 * helpers for those.
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
 * Clears MCUCTRL DBGCTRL and powers down the debug domain.
 * Call before entering measurement — debug logic draws current.
 */
void nsx_power_disable_debug(void);

/**
 * @brief Tristate all GPIO pads except specified pins.
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
