/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, Ambiq
 */
#include "nsx_freertos.h"

#include "FreeRTOS.h"
#include "task.h"

const char *nsx_freertos_kernel_version(void)
{
    return tskKERNEL_VERSION_NUMBER;
}

void nsx_freertos_start(void)
{
    vTaskStartScheduler();
}
