/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, Ambiq
 *
 * nsx-freertos thin convenience API.
 *
 * This header is intentionally minimal. Applications that need the full
 * kernel API should include the FreeRTOS headers (FreeRTOS.h, task.h,
 * queue.h, ...) directly. These helpers only wrap the few operations that
 * benefit from an NSX-stable name.
 */
#ifndef NSX_FREERTOS_H
#define NSX_FREERTOS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the vendored FreeRTOS kernel version string (e.g. "V11.1.0").
 */
const char *nsx_freertos_kernel_version(void);

/**
 * @brief Start the FreeRTOS scheduler.
 *
 * Wraps vTaskStartScheduler(). Does not return under normal operation. The
 * application is expected to have created at least one task beforehand.
 */
void nsx_freertos_start(void);

#ifdef __cplusplus
}
#endif

#endif /* NSX_FREERTOS_H */
