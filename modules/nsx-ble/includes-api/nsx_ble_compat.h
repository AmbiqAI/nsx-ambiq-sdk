/**
 * @file nsx_ble_compat.h
 * @brief Thin compatibility shim mapping legacy neuralSPOT ns-ble harness
 *        symbols onto neuralSPOT-X (NSX) equivalents.
 *
 * The ns-ble wrapper was written against the old neuralSPOT platform helpers
 * (HAL umbrella includes + ns_malloc.h). Under NSX those facilities are
 * provided by nsx-core (logging, interrupt master enable/disable, status
 * codes, API version negotiation) and by FreeRTOS (heap). This header lets the
 * ported ns_ble.c stay almost verbatim while resolving those names to NSX.
 */
#ifndef NSX_BLE_COMPAT_H
#define NSX_BLE_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// NSX core: nsx_printf / nsx_low_power_printf / nsx_interrupt_master_*,
// nsx_core_api_t, NSX_STATUS_*, nsx_semver_t.
#include "nsx_core.h"

// AmbiqSuite HAL umbrella header. The legacy harness pulled this in; ns_ble.c
// needs CMSIS/HAL symbols directly for the BLE transport helpers.
#include "am_mcu_apollo.h"

// The WSF port is the FreeRTOS port; use the RTOS heap for the small dynamic
// allocations ns-ble makes (characteristic/handle bookkeeping).
#include "FreeRTOS.h"
#include "task.h"

// --- Logging -------------------------------------------------------------
#ifndef ns_lp_printf
    #define ns_lp_printf nsx_low_power_printf
#endif

// --- Interrupt master enable/disable ------------------------------------
#ifndef ns_interrupt_master_enable
    #define ns_interrupt_master_enable nsx_interrupt_master_enable
#endif
#ifndef ns_interrupt_master_disable
    #define ns_interrupt_master_disable nsx_interrupt_master_disable
#endif

// --- Heap ---------------------------------------------------------------
#ifndef ns_malloc
    #define ns_malloc pvPortMalloc
#endif
#ifndef ns_free
    #define ns_free vPortFree
#endif

// --- Status codes -------------------------------------------------------
#ifndef NS_STATUS_SUCCESS
    #define NS_STATUS_SUCCESS NSX_STATUS_SUCCESS
#endif
#ifndef NS_STATUS_FAILURE
    #define NS_STATUS_FAILURE NSX_STATUS_FAILURE
#endif

// --- Core API version struct -------------------------------------------
// Legacy ns_core_api_t and nsx_core_api_t are layout-identical:
//   { uint32_t apiId; <semver> version; }  with version = {major,minor,revision}.
typedef nsx_core_api_t ns_core_api_t;

// --- Misc helpers used by ns_ble.c --------------------------------------
#ifndef NS_TRY
    #define NS_TRY(func, msg) NSX_TRY_ABORT(func, msg)
#endif

#ifndef NS_SAFE_MEMCPY
    #define NS_SAFE_MEMCPY memcpy
#endif

#endif // NSX_BLE_COMPAT_H
