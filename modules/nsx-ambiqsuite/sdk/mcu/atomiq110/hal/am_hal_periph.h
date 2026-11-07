//*****************************************************************************
//
//! @file am_hal_periph.h
//!
//! @brief Common interface for RTOS aware peripheral HAL driver
//!
//! @addtogroup periph_ap6xx
//! @ingroup ap6xx_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_PERIPH_H
#define AM_HAL_PERIPH_H

#include "cmsis_os2.h"
#include "cmsis_types.h"
#include "am_hal_status.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned int uint;

//*****************************************************************************
//
//! @brief Common peripheral Types.
//! @defgroup periph_types_ap6xx
//! @ingroup periph_ap6xx
//! @{
//
//*****************************************************************************

#define AM_HAL_PERIPH_TIMEOUT_NO_WAIT 0x00000000u
#define AM_HAL_PERIPH_TIMEOUT_FOREVER 0xFFFFFFFFu

//*****************************************************************************
//
//! @brief Exposed Common types
//
//*****************************************************************************
typedef struct cmsis_rtos_mutex_cb     am_hal_periph_lock_t;
typedef struct cmsis_rtos_semaphore_cb am_hal_periph_token_t;

#define AM_HAL_PERIPH_LOCK_CB_SIZE   (sizeof(am_hal_periph_lock_t))
#define AM_HAL_PERIPH_TOKEN_CB_SIZE  (sizeof(am_hal_periph_token_t))

typedef struct periph_state  am_hal_periph_state_t;
typedef struct periph_ops    am_hal_periph_ops_t;
typedef struct periph_handle am_hal_periph_handle_t;


struct periph_state
{
    //
    //! Device initialization return code.
    //
    uint8_t              ui8InitRet;

    //
    //! Indicates the device initialization function has been invoked.
    //
    bool                 bInitialized;

    //
    //! Indicates whether periph is thread locked.
    //
    am_hal_periph_lock_t lock;
};


struct periph_handle
{
    //
    //! Buffer for device instance information
    //
    const void                *pData;
    unsigned int              uintSize;

    //
    //! High-level HAL dependency (array of handles)
    //
    const void                *pDeps;

    //
    //! Address of the API structure exposed by the device instance
    //
    const void                *pApi;

    //
    //! Address of the common state
    //
    am_hal_periph_state_t     state;

    //
    //! Common peripheral operations
    //
    const am_hal_periph_ops_t *pOps;

    //
    //! Module instance number
    //
    uint8_t                   ui8ModuleNum;
};

//*****************************************************************************
//
//! @brief Common peripheral operations
//
//*****************************************************************************
struct periph_ops
{

    uint (*pre_init)(am_hal_periph_handle_t *periph);

    uint (*post_deinit)(am_hal_periph_handle_t *periph);

    uint (*acquire)(am_hal_periph_handle_t *periph, uint timeout);

    uint (*release)(am_hal_periph_handle_t *periph);

    uint (*delay)(am_hal_periph_handle_t *periph, uint delayUS);

    // Return OS tick period in microseconds (0 if not available).
    uint (*tick_us)(am_hal_periph_handle_t *periph);

    uint (*token_init)(am_hal_periph_token_t *token, uint max, uint init);

    uint (*token_deinit)(am_hal_periph_token_t *token);

    uint (*token_wait)(am_hal_periph_token_t *token, uint timeout);

    uint (*token_notify)(am_hal_periph_token_t *token);

};

//! @} END_GROUP - Common peripheral Types.

extern const am_hal_periph_ops_t sOPS;

//*****************************************************************************
//
//! @brief Common peripheral Helper Macro.
//! @defgroup periph_macros_ap6xx
//! @ingroup periph_ap6xx
//! @{
//
//*****************************************************************************
#define _CAT2(a, b) a ## b
#define _CAT3(a, b, c) a ## b ## c
#define _CAT4(a, b, c, d) a ## b ## c ## d

#define EXPAND2(a, b) _CAT2(a, b)
#define EXPAND3(a, b, c) _CAT3(a, b, c)
#define EXPAND4(a, b, c, d) _CAT4(a, b, c, d)

#define AM_HAL_HANDLE(MODULE_NAME, MODULE) EXPAND2(MODULE_NAME, MODULE)
#define AM_HAL_HANDLE_NAME(MODULE_NAME, MODULE)                                      \
        EXPAND3(s, AM_HAL_HANDLE(MODULE_NAME, MODULE), Handle)

#define AM_HAL_HANDLE_DEFINE(ATTR, MODULE_NAME, MODULE, SIZE, API, DEPS)             \
        ATTR uint8_t EXPAND3(ui8, AM_HAL_HANDLE(MODULE_NAME, MODULE), Data[SIZE]);   \
        ATTR am_hal_periph_handle_t                                                  \
        AM_HAL_HANDLE_NAME(MODULE_NAME, MODULE) =                                    \
        {                                                                            \
            .pData        = EXPAND3(ui8, AM_HAL_HANDLE(MODULE_NAME, MODULE), Data),  \
            .uintSize     = SIZE,                                                    \
            .pOps         = &sOPS,                                                   \
            .ui8ModuleNum = MODULE,                                                  \
            .pDeps        = (DEPS),                                                  \
            .pApi         = (API),                                                   \
        }

#define AM_HAL_HANDLE_INITIALIZE(MODULE, DATA, DATASIZE, OPS, API, DEPS)             \
        {                                                                            \
            .pData        = DATA,                                                    \
            .uintSize     = DATASIZE,                                                \
            .pOps         = (OPS),                                                   \
            .ui8ModuleNum = MODULE,                                                  \
            .pDeps        = (DEPS),                                                  \
            .pApi         = (API),                                                   \
        }

#define AM_HAL_HANDLE_PRE_KERNEL_OPS()                                               \
        {                                                                            \
            .pre_init     = NULL,                                                    \
            .acquire      = NULL,                                                    \
            .release      = NULL,                                                    \
            .delay        = am_hal_delay_us,                                         \
            .tick_us      = NULL,                                                    \
            .token_init   = NULL,                                                    \
            .token_deinit = NULL,                                                    \
            .token_wait   = NULL,                                                    \
            .token_notify = NULL,                                                    \
        }

// (am_hal_periph_tick_us declared after AM_HAL_PERIPH_CHK_HANDLE so it can be
// used in both kernel and pre-kernel builds without relying on return-code
// macros.)

#define AM_HAL_PERIPH_CHK_HANDLE(pHandle)                                            \
        {                                                                            \
            if (!pHandle || !pHandle->state.bInitialized)                            \
            {                                                                        \
                return AM_HAL_STATUS_INVALID_HANDLE;                                 \
            }                                                                        \
        }

//*****************************************************************************
//
//! @brief Peripheral HAL tick period query.
//!
//! @param periph      - the handle for the module instance.
//!
//! @return tick_us     - OS tick period in microseconds, or 0 if unavailable.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_tick_us(am_hal_periph_handle_t *periph)
{
    if (!periph || !periph->state.bInitialized)
    {
        return 0u;
    }

    const am_hal_periph_ops_t *pOps = periph->pOps;
    if (!pOps || !pOps->tick_us)
    {
        return 0u;
    }
    return pOps->tick_us(periph);
}

//! @} END_GROUP - Common peripheral Helper Macro.

//*****************************************************************************
//
//! @brief Peripheral HAL acquire API entry point
//!
//! @param pHandle        - the handle for the module instance.
//! @param ui32TimeoutUs  - the timeout in microseconds.
//!
//! This function accepts a handle to an instance and enable the
//! interface with a timeout.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_acquire(am_hal_periph_handle_t *periph, uint32_t ui32TimeoutUs)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->acquire)
    {
        return 0;
    }

    return pOps->acquire(periph, ui32TimeoutUs);
}

//*****************************************************************************
//
//! @brief Peripheral HAL release API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//!
//! This function accepts a handle to an instance and disables the
//! interface.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_release(am_hal_periph_handle_t *periph)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->release)
    {
        return 0;
    }

    return pOps->release(periph);
}

//*****************************************************************************
//
//! @brief Peripheral HAL Delay API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//! @param ui32DelayUs  - delay in microseconds.
//!
//! This function accepts a handle to an instance and execute cycle or tick delay
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_delay(am_hal_periph_handle_t *periph, uint32_t ui32DelayUs)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->delay)
    {
        return 0;
    }

    return pOps->delay(periph, ui32DelayUs);
}

//*****************************************************************************
//
//! @brief Peripheral HAL pre-init API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//!
//! This function accepts a handle to an instance and handles resource
//! initialization and/or allocation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_preinit(am_hal_periph_handle_t *periph)
{
    if (!periph || periph->state.bInitialized)
    {
        return AM_HAL_STATUS_INVALID_HANDLE;
    }

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->pre_init)
    {
        return 0;
    }

    return pOps->pre_init(periph);
}

//*****************************************************************************
//
//! @brief Peripheral HAL post-init API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//!
//! This function accepts a handle to an instance and handles resource
//! deinitialization and/or deallocation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_postdeinit(am_hal_periph_handle_t *periph)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->post_deinit)
    {
        return 0;
    }

    return pOps->post_deinit(periph);
}

//*****************************************************************************
//
//! @brief Peripheral HAL token init API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//! @param pToken       - the token to be initialized.
//! @param ui32MaxCnt   - the maximum count of the token.
//! @param ui32InitCnt  - the initial count of the token.
//!
//! This function accepts a handle to an instance and handles resource
//! initialization and/or allocation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_token_init(am_hal_periph_handle_t *periph,
                                                am_hal_periph_token_t *pToken,
                                                uint32_t ui32MaxCnt, uint32_t ui32InitCnt)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->token_init)
    {
        return 0;
    }

    return pOps->token_init(pToken, ui32MaxCnt, ui32InitCnt);
}

//*****************************************************************************
//
//! @brief Peripheral HAL token deinit API entry point.
//!
//! @param pHandle      - the handle for the module instance.
//! @param pToken       - the token to be deinitialized.
//!
//! This function accepts a handle to an instance and handles resource
//! deinitialization and/or deallocation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_token_deinit(am_hal_periph_handle_t *periph,
                                                  am_hal_periph_token_t *pToken)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->token_deinit)
    {
        return 0;
    }

    return pOps->token_deinit(pToken);
}

//*****************************************************************************
//
//! @brief Peripheral HAL token wait API entry point.
//!
//! @param pHandle        - the handle for the module instance.
//! @param pToken         - the token to be waited on.
//! @param ui32TimeoutUs  - the timeout in microseconds.
//!
//! This function try to wait for a token with a timeout.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_token_wait(am_hal_periph_handle_t *periph,
                                                am_hal_periph_token_t *pToken,
                                                uint32_t ui32TimeoutUs)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->token_wait)
    {
        return 0;
    }

    return pOps->token_wait(pToken, ui32TimeoutUs);
}

//*****************************************************************************
//
//! @brief Peripheral HAL token notify entry point.
//!
//! @param pHandle      - the handle for the module instance.
//! @param pToken       - the token to be notified.
//!
//! This function notify the given token.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
static inline uint32_t am_hal_periph_token_notify(am_hal_periph_handle_t *periph,
                                                  am_hal_periph_token_t *pToken)
{
    AM_HAL_PERIPH_CHK_HANDLE(periph)

    const am_hal_periph_ops_t *pOps = periph->pOps;

    if (!pOps->token_notify)
    {
        return 0;
    }

    return pOps->token_notify(pToken);
}

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_PERIPH_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
