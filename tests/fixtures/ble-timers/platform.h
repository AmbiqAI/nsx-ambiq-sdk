/* Native boundary replacements for nsx_ble_compat.h. Public BLE and Cordio
 * headers remain real; no radio, RTOS, or interrupt hardware is exercised. */
#ifndef TEST_BLE_PLATFORM_H
#define TEST_BLE_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define NSX_BLE_COMPAT_H
#define AM_HAL_STATUS_SUCCESS 0
#define NS_STATUS_SUCCESS 0
#define NS_STATUS_FAILURE 1
#define ns_malloc malloc
#define ns_free free
#define NS_SAFE_MEMCPY memcpy
#define NS_TRY(func,msg) do { if ((func) != NS_STATUS_SUCCESS) return NS_STATUS_FAILURE; } while(0)
typedef struct { uint32_t apiId; struct { unsigned major, minor, revision; } version; } ns_core_api_t;
typedef int IRQn_Type;
void ns_lp_printf(const char *, ...);
void ns_interrupt_master_disable(void);
void ns_interrupt_master_enable(void);

#endif
