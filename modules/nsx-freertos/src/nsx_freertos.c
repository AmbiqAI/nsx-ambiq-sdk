/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, Ambiq
 */
#include "nsx_freertos.h"

#include "FreeRTOS.h"
#include "task.h"

#if defined(NSX_SOC_CORE_M4)
/*
 * The ARM_CM4F port uses indirect handler routing (configCHECK_HANDLER_INSTALLATION
 * is 0): NSX startup owns the SVC_Handler / PendSV_Handler / SysTick_Handler
 * vector slots and nsx-freertos provides strong shim handlers that branch to the
 * upstream vPortSVCHandler / xPortPendSVHandler / xPortSysTickHandler entry points
 * (see nsx_freertos_handlers_cm4f.c). This referenced symbol forces the shim
 * translation unit to be pulled out of the static library archive so those strong
 * handlers override the weak startup stubs.
 */
void nsx_freertos_force_cm4f_handlers(void);
#endif

const char *nsx_freertos_kernel_version(void)
{
    return tskKERNEL_VERSION_NUMBER;
}

void nsx_freertos_start(void)
{
#if defined(NSX_SOC_CORE_M4)
    nsx_freertos_force_cm4f_handlers();
#endif
    vTaskStartScheduler();
}
