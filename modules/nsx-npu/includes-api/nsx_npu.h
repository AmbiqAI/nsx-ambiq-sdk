/**
 * @file nsx_npu.h
 * @author Ambiq
 * @brief NSX Arm Ethos-U85 NPU driver helpers for Atomiq targets.
 * @version 0.1
 * @date 2026-08-06
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 *
 * \addtogroup nsx-npu
 *  @{
 */

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
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
//*****************************************************************************

#ifndef NSX_NPU_H
#define NSX_NPU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

struct ethosu_driver;

/**
 * @brief NPU performance mode.
 */
typedef enum {
    NSX_NPU_PERF_ULTRA_LOW_POWER, ///< 100 MHz NPU clock
    NSX_NPU_PERF_HIGH_PERFORMANCE ///< 500 MHz NPU clock
} nsx_npu_perf_mode_e;

/**
 * @brief NPU configuration.
 */
typedef struct {
    nsx_npu_perf_mode_e perf_mode; ///< Requested NPU performance mode.
    bool skip_perf_mode;           ///< Skip performance-mode programming
                                   ///< (e.g. FPGA/pre-silicon targets).
    bool tolerate_power_ack;       ///< Tolerate a failed NPU power-domain
                                   ///< enable handshake instead of failing
                                   ///< init. FPGA images keep the NPU
                                   ///< always-on and do not model the
                                   ///< power-status ack; set this for
                                   ///< FPGA/pre-silicon targets only. On
                                   ///< silicon, leave false so a genuine
                                   ///< power-up failure returns
                                   ///< `NSX_STATUS_INIT_FAILED` instead of
                                   ///< proceeding to a bus fault.
} nsx_npu_config_t;

/**
 * @brief Power on the Ethos-U85, initialize the core driver, and enable the
 * NPU interrupt.
 *
 * Registers the module-owned `am_npu_isr` -> `ethosu_irq_handler` glue and
 * leaves the driver ready for `ethosu_invoke`-style dispatch (directly or via
 * a TFLM ethos-u custom op kernel).
 *
 * @param cfg NPU configuration. NULL selects high performance mode.
 * @return `NSX_STATUS_SUCCESS` on success, otherwise an NSX status code.
 */
extern uint32_t nsx_npu_init(const nsx_npu_config_t *cfg);

/**
 * @brief Deinitialize the driver, disable the NPU interrupt, and power the
 * NPU domain down.
 *
 * @return `NSX_STATUS_SUCCESS` on success, otherwise an NSX status code.
 */
extern uint32_t nsx_npu_deinit(void);

/**
 * @brief Access the module-owned Ethos-U driver handle.
 *
 * Intended for advanced use such as PMU configuration or direct
 * `ethosu_invoke` dispatch.
 *
 * @return Driver handle, or NULL when `nsx_npu_init` has not succeeded.
 */
extern struct ethosu_driver *nsx_npu_driver(void);

#ifdef __cplusplus
}
#endif

#endif // NSX_NPU_H

/** @}*/
