/**
 * @file nsx_core.h
 * @brief NSX core runtime and lifecycle API.
 *
 * Declares the portable runtime helpers (printf/delay/interrupt control) and
 * the core lifecycle API (API-version negotiation, init, status codes, and
 * unaligned-safe memcpy helpers) shared by all NSX modules.
 *
 * @copyright Copyright (c) 2024-2026, Ambiq Micro, Inc.
 *
 * \addtogroup nsx-core
 * @{
 */
#ifndef NSX_CORE_H
#define NSX_CORE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nsx_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Portable runtime helpers
 * =================================================================== */
void nsx_printf(const char *fmt, ...);
void nsx_low_power_printf(const char *fmt, ...);
void nsx_delay_us(uint32_t usec);
void nsx_interrupt_master_enable(void);
void nsx_interrupt_master_disable(void);
void nsx_debug_printf_enable(void);
void nsx_debug_printf_disable(void);
void nsx_itm_printf_enable(void);
void nsx_itm_printf_disable(void);
void nsx_uart_printf_enable(void);
void nsx_uart_printf_disable(void);

/* ===================================================================
 * API version negotiation
 * =================================================================== */
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t revision;
} nsx_semver_t;

typedef struct {
    uint32_t apiId;
    nsx_semver_t version;
} nsx_core_api_t;

#define NSX_CORE_V0_0_1                                                        \
    { .major = 0, .minor = 0, .revision = 1 }
#define NSX_CORE_V1_0_0                                                        \
    { .major = 1, .minor = 0, .revision = 0 }
#define NSX_CORE_OLDEST_SUPPORTED_VERSION NSX_CORE_V0_0_1
#define NSX_CORE_CURRENT_VERSION NSX_CORE_V1_0_0

extern const nsx_core_api_t nsx_core_V0_0_1;
extern const nsx_core_api_t nsx_core_V1_0_0;
extern const nsx_core_api_t nsx_core_oldest_supported_version;
extern const nsx_core_api_t nsx_core_current_version;

// #define NSX_DISABLE_API_VALIDATION
#define NSX_STATUS_SUCCESS 0
#define NSX_STATUS_FAILURE -1
#define NSX_STATUS_INVALID_HANDLE 1
#define NSX_STATUS_INVALID_VERSION 2
#define NSX_STATUS_INVALID_CONFIG 3
#define NSX_STATUS_INIT_FAILED 4

#define NSX_TRY(func, msg)                                                     \
    if (func) {                                                                \
        nsx_printf(msg);                                                       \
        nsx_core_fail_loop();                                                  \
    }

#define NSX_TRY_ABORT(func, msg)                                               \
    if (func) {                                                                \
        nsx_printf(msg);                                                       \
        nsx_printf("\n");                                                      \
        return NSX_STATUS_FAILURE;                                             \
    }

#if defined(AM_PART_APOLLO5B) || defined(AM_PART_APOLLO4L) ||                  \
    defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO510L) ||                \
    defined(AM_PART_APOLLO330P)
    #define NSX_SRAM_BSS NSX_SECTION(".sram_bss")
#else
    #define NSX_SRAM_BSS
#endif

/* ===================================================================
 * Lifecycle
 * =================================================================== */

/**
 * @brief Core config struct
 */
typedef struct {
    const nsx_core_api_t *api; ///< Core API version
} nsx_core_config_t;

/**
 * @brief Query whether nsx_core_init() has completed.
 *
 * Introspection helper only. Peripheral modules (GPIO, UART, timer,
 * interrupt, power) must NOT use this as an init gate — each module is
 * responsible for validating only its own prerequisites.
 *
 * @return true if nsx_core_init() has run, false otherwise.
 */
extern bool nsx_core_initialized(void);

/**
 * @brief Initialize core state.
 *
 * @param c nsx_core_config_t indicating the API version
 * @return uint32_t status code
 */
extern uint32_t nsx_core_init(nsx_core_config_t *c);

extern uint32_t nsx_core_check_api(
    const nsx_core_api_t *submitted, const nsx_core_api_t *oldest,
    const nsx_core_api_t *newest);

extern void nsx_core_fail_loop(void);

/* ===================================================================
 * Unaligned-safe memcpy helpers
 * =================================================================== */

static inline void *nsx_memcpy_u8(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) { *d++ = *s++; }
    return dst;
}

// Safe memcpy that falls back to byte copy if either pointer is not 4-byte
// aligned.
static inline void *nsx_memcpy_safe(void *dst, const void *src, size_t n) {
    const uintptr_t a = ((uintptr_t)dst | (uintptr_t)src);
    if (((a & 0x3u) == 0u) && (n >= 4u)) {
        // Both pointers 4-aligned — let the C library do a fast copy.
        return memcpy(dst, src, n);
    }
    // Otherwise do a byte-wise copy that cannot fault with UNALIGN_TRP=1.
    return nsx_memcpy_u8(dst, src, n);
}

// Use this macro everywhere you would have used memcpy for possibly-unaligned
// inputs.
#define NSX_SAFE_MEMCPY(DST, SRC, N) nsx_memcpy_safe((DST), (SRC), (N))

#ifdef __cplusplus
}
#endif

#endif /* NSX_CORE_H */
/** @}*/
