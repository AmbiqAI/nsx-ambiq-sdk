/**
 * @brief Power Control Utilities
 *
 *
 */

//*****************************************************************************
//
// Copyright (c) Ambiq Micro, Inc.
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
// This source is staged for AmbiqSuite R5-backed NSX targets.
//
//*****************************************************************************

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power.h"

const nsx_core_api_t nsx_power_V0_0_1 = {.apiId = NSX_POWER_API_ID, .version = NSX_POWER_V0_0_1};

const nsx_core_api_t nsx_power_V1_0_0 = {.apiId = NSX_POWER_API_ID, .version = NSX_POWER_V1_0_0};

const nsx_core_api_t nsx_power_oldest_supported_version = {
    .apiId = NSX_POWER_API_ID, .version = NSX_POWER_V0_0_1};

const nsx_core_api_t nsx_power_current_version = {
    .apiId = NSX_POWER_API_ID, .version = NSX_POWER_V1_0_0};

const nsx_power_config_t nsx_power_all_on = {
    .perf_mode = NSX_POWER_PERF_HIGH,
    .api = &nsx_power_V1_0_0,
    .need_audadc = true,
    .need_ssram = true,
    .need_crypto = false,
    .need_ble = true,
    .need_usb = true,
    .need_iom = true,
    .need_uart = true,
    .small_tcm = false,
    .need_tempco = false,
    .need_itm = true,
    .need_xtal = true,
    .spotmgr_collapse = true};

const nsx_power_config_t nsx_power_minimal = {
    .perf_mode = NSX_POWER_PERF_LOW,
    .api = &nsx_power_V1_0_0,
    .need_audadc = false,
    .need_ssram = false,
    .need_crypto = false,
    .need_ble = false,
    .need_usb = false,
    .need_iom = false,
    .need_uart = false,
    .small_tcm = false,
    .need_tempco = false,
    .need_itm = false,
    .need_xtal = false,
    .spotmgr_collapse = true};

const nsx_power_config_t nsx_power_audio = {
    .perf_mode = NSX_POWER_PERF_HIGH,
    .api = &nsx_power_V1_0_0,
    .need_audadc = true,
    .need_ssram = false,
    .need_crypto = false,
    .need_ble = false,
    .need_usb = false,
    .need_iom = false,
    .need_uart = false,
    .small_tcm = false,
    .need_tempco = false,
    .need_itm = false,
    .need_xtal = false,
    .spotmgr_collapse = true};

extern uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t mode);
extern uint32_t nsx_power_platform_config(const nsx_power_config_t *pCfg);

// Main function for power configuration
uint32_t nsx_power_configure(const nsx_power_config_t *pCfg) {
    uint32_t ui32ReturnStatus = AM_HAL_STATUS_SUCCESS;

#ifndef NSX_DISABLE_API_VALIDATION
    if (pCfg == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    if (nsx_core_check_api(
            pCfg->api, &nsx_power_oldest_supported_version, &nsx_power_current_version)) {
        return NSX_STATUS_INVALID_VERSION;
    }
#endif

    ui32ReturnStatus = nsx_power_platform_config(pCfg);

    return ui32ReturnStatus;
}

/**
 * @brief Wraps am_hal_sysctrl_sleep to enable and disable
 * systems as needed.
 *
 */
extern void nsx_power_platform_deep_sleep(void);

void nsx_power_deep_sleep(void) { nsx_power_platform_deep_sleep(); }
