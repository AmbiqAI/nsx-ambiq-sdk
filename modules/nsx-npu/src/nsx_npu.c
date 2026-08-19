/**
 * @file nsx_npu.c
 * @author Ambiq
 * @brief NSX Arm Ethos-U85 NPU driver helpers for Atomiq targets.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#include "nsx_npu.h"

#include "am_mcu_apollo.h"
#include "nsx_core.h"
#include "nsx_ethos_u.h"

//*****************************************************************************
//
// Module state
//
//*****************************************************************************

// Ethos-U register base. The Atomiq110 NPU decodes at the non-secure alias
// 0x400E0000 (hardware-validated; matches the AmbiqSuite npu_resnet FPGA
// example). The secure alias 0x500E0000 bus-faults on the FPGA image.
#define NSX_NPU_REG_BASE (0x400E0000UL)

static struct ethosu_driver g_nsx_npu_driver;
static bool g_nsx_npu_initialized = false;

//*****************************************************************************
//
// Interrupt glue
//
//*****************************************************************************

void am_npu_isr(void)
{
    nsx_ethos_u_irq();
}

//*****************************************************************************
//
// Public API
//
//*****************************************************************************

// nsx-npu owns NPU power-domain sequencing exclusively: nsx-ethos-u-driver
// never touches am_hal_pwrctrl.
uint32_t nsx_npu_init(const nsx_npu_config_t *cfg)
{
    if (g_nsx_npu_initialized)
    {
        return NSX_STATUS_SUCCESS;
    }

    //
    // Power on the NPU domain. Tolerate a failed handshake only if
    // cfg->tolerate_power_ack is set (FPGA/pre-silicon targets); otherwise
    // fail init here rather than bus-fault later on the register block.
    //
    if ((am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_NPU) != AM_HAL_STATUS_SUCCESS) &&
        ((cfg == NULL) || !cfg->tolerate_power_ack))
    {
        return NSX_STATUS_INIT_FAILED;
    }

    //
    // Initialize the Ethos-U core driver. Secure/privileged=1 matches the
    // AmbiqSuite Atomiq110 NPU examples.
    //
    if (nsx_ethos_u_init(&g_nsx_npu_driver, (void *)NSX_NPU_REG_BASE, NPU_IRQn) != 0)
    {
        (void)am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_NPU);
        return NSX_STATUS_INIT_FAILED;
    }

    //
    // Route the NPU interrupt to the driver and enable it.
    //
    NVIC_SetPriority(NPU_IRQn, AM_IRQ_PRIORITY_DEFAULT);
    NVIC_ClearPendingIRQ(NPU_IRQn);
    NVIC_EnableIRQ(NPU_IRQn);
    am_hal_interrupt_master_enable();

    //
    // Select the NPU performance mode. Optional: FPGA/pre-silicon targets can
    // skip it because the clock tree is fixed by the image.
    //
    if ((cfg == NULL) || !cfg->skip_perf_mode)
    {
        am_hal_pwrctrl_npu_mode_e mode = AM_HAL_PWRCTRL_NPU_MODE_HIGH_PERFORMANCE;
        if ((cfg != NULL) && (cfg->perf_mode == NSX_NPU_PERF_ULTRA_LOW_POWER))
        {
            mode = AM_HAL_PWRCTRL_NPU_MODE_ULTRA_LOW_POWER;
        }
        if (am_hal_pwrctrl_npu_mode_select(mode) != AM_HAL_STATUS_SUCCESS)
        {
            NVIC_DisableIRQ(NPU_IRQn);
            __DSB();
            __ISB();
            nsx_ethos_u_deinit();
            ethosu_deinit(&g_nsx_npu_driver);
            (void)am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_NPU);
            return NSX_STATUS_INIT_FAILED;
        }
    }

    g_nsx_npu_initialized = true;
    return NSX_STATUS_SUCCESS;
}

uint32_t nsx_npu_deinit(void)
{
    if (!g_nsx_npu_initialized)
    {
        return NSX_STATUS_SUCCESS;
    }

    NVIC_DisableIRQ(NPU_IRQn);
    __DSB();
    __ISB();

    // The IRQ is masked and any already-pended interrupt cannot reach the
    // handler once the driver handle below is cleared. From this point on
    // the module is deinitialized regardless of what PWRCTRL reports, so
    // clear state before the power-disable call rather than after it.
    nsx_ethos_u_deinit();
    ethosu_deinit(&g_nsx_npu_driver);
    g_nsx_npu_initialized = false;

    if (am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_NPU) != AM_HAL_STATUS_SUCCESS)
    {
        return NSX_STATUS_FAILURE;
    }

    return NSX_STATUS_SUCCESS;
}

struct ethosu_driver *nsx_npu_driver(void)
{
    return g_nsx_npu_initialized ? &g_nsx_npu_driver : NULL;
}
