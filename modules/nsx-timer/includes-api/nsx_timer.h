/**
 * @file nsx_timer.h
 * @author Ambiq
 * @brief Simple timer facility
 * @version 0.1
 * @date 2022-10-11
 *
 * @copyright Copyright (c) 2022-2026, Ambiq Micro, Inc.
 *
 * \addtogroup nsx-timer
 * @{
 * @ingroup nsx-timer
 *
 */

#ifndef NSX_TIMER_H
#define NSX_TIMER_H

    #ifdef __cplusplus
extern "C" {
    #endif

    #include "nsx_core.h"

    #define NSX_TIMER_V0_0_1                                                                        \
        { .major = 0, .minor = 0, .revision = 1 }
    #define NSX_TIMER_V1_0_0                                                                        \
        { .major = 1, .minor = 0, .revision = 0 }

    #define NSX_TIMER_OLDEST_SUPPORTED_VERSION NSX_TIMER_V0_0_1
    #define NSX_TIMER_CURRENT_VERSION NSX_TIMER_V1_0_0
    #define NSX_TIMER_API_ID 0xCA0002

extern const nsx_core_api_t nsx_timer_V0_0_1;
extern const nsx_core_api_t nsx_timer_V1_0_0;
extern const nsx_core_api_t nsx_timer_oldest_supported_version;
extern const nsx_core_api_t nsx_timer_current_version;

struct nsx_timer_config;
typedef void (*nsx_timer_callback_cb)(struct nsx_timer_config *);

/**
 * @brief Supported Timers
 *
 */
typedef enum {
    NSX_TIMER_COUNTER = 0,   ///< Intended use is reading timerticks
    NSX_TIMER_INTERRUPT = 1, ///< Calls a callback periodically
    NSX_TIMER_USB = 3,       ///< Used by ns_usb to periodically service USB
    NSX_TIMER_TEMPCO = 4     ///< Used by ns_tempco to periodically collect temps
} nsx_timers_e;

/**
 * @brief ns-timer configuration struct
 *
 */
typedef struct nsx_timer_config {
    const nsx_core_api_t *api;      ///< API prefix
    nsx_timers_e timer;             ///< NSX_TIMER_COUNTER, NSX_TIMER_INTERRUPT, or NSX_TIMER_USB
    bool enableInterrupt;          ///< Will generate interrupts, needs callback
    uint32_t periodInMicroseconds; ///< For interrupts
    nsx_timer_callback_cb callback; ///< Invoked when timer ticks
} nsx_timer_config_t;

/**
 * @brief Initialize one of 3 timers supported by NSX
 *
 * NSX_TIMER_COUNTER     Intended use is reading timerticks
 * NSX_TIMER_INTERRUPT   Calls a callback periodically
 * NSX_TIMER_USB         Used by ns_usb to periodically service USB
 * NSX_TIMER_TEMPCO      Used by ns_tempco to periodically collect temps
 *
 * @param cfg
 * @return uint32_t status
 */
extern uint32_t nsx_timer_init(nsx_timer_config_t *cfg);

/**
 * @brief Read current value of timer
 *
 * @param cfg
 * @return uint32_t timer if success, 0xDEADBEEF if bad handle
 */
extern uint32_t nsx_timer_us_read(nsx_timer_config_t *cfg);

/**
 * @brief Clear timer
 *
 * @param cfg
 * @return uint32_t status
 */
extern uint32_t nsx_timer_clear(nsx_timer_config_t *cfg);

    #ifdef __cplusplus
}
    #endif
#endif
/** @}*/
