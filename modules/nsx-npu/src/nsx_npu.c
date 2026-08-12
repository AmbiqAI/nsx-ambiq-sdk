/**
 * @file nsx_npu.c
 * @author Ambiq
 * @brief NSX Arm Ethos-U85 NPU driver helpers for Atomiq targets.
 *
 * @copyright Copyright (c) 2026, Ambiq Micro, Inc.
 */

#include "nsx_npu.h"

#include "am_mcu_apollo.h"
#include "ethosu_driver.h"
#include "nsx_core.h"

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
static struct ethosu_driver *g_nsx_npu_irq_driver = NULL;
static bool g_nsx_npu_initialized = false;

//*****************************************************************************
//
// Interrupt glue
//
//*****************************************************************************

void am_npu_isr(void)
{
    if (g_nsx_npu_irq_driver != NULL)
    {
        ethosu_irq_handler(g_nsx_npu_irq_driver);
    }
}

//*****************************************************************************
//
// Public API
//
//*****************************************************************************

uint32_t nsx_npu_init(const nsx_npu_config_t *cfg)
{
    if (g_nsx_npu_initialized)
    {
        return NSX_STATUS_SUCCESS;
    }

    //
    // Power on the NPU domain. Tolerate a failed handshake: FPGA images keep
    // the NPU always-on and do not model the power-status ack (the AmbiqSuite
    // npu_resnet example behaves the same way).
    //
    (void)am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_NPU);

    //
    // Initialize the Ethos-U core driver. Secure=0/privileged=1 matches the
    // AmbiqSuite Atomiq110 NPU examples.
    //
    if (ethosu_init(&g_nsx_npu_driver, (void *)NSX_NPU_REG_BASE, NULL, 0, 0, 1) != 0)
    {
        (void)am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_NPU);
        return NSX_STATUS_INIT_FAILED;
    }

    //
    // Route the NPU interrupt to the driver and enable it.
    //
    g_nsx_npu_irq_driver = &g_nsx_npu_driver;
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
            g_nsx_npu_irq_driver = NULL;
            NVIC_DisableIRQ(NPU_IRQn);
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
    g_nsx_npu_irq_driver = NULL;
    ethosu_deinit(&g_nsx_npu_driver);

    if (am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_NPU) != AM_HAL_STATUS_SUCCESS)
    {
        return NSX_STATUS_FAILURE;
    }

    g_nsx_npu_initialized = false;
    return NSX_STATUS_SUCCESS;
}

struct ethosu_driver *nsx_npu_driver(void)
{
    return g_nsx_npu_initialized ? &g_nsx_npu_driver : NULL;
}
