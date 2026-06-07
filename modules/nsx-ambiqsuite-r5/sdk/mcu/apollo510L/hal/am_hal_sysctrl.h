//*****************************************************************************
//
//! @file am_hal_sysctrl.h
//!
//! @brief Functions for interfacing with the M55 system control registers
//!
//! @addtogroup sysctrl4_ap510L SYSCTRL - System Control
//! @ingroup apollo510L_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_SYSCTRL_H
#define AM_HAL_SYSCTRL_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Definitions for sleep mode parameter
//! @{
//
//*****************************************************************************
typedef enum
{
  AM_HAL_SYSCTRL_SLEEP_NORMAL = 0,
  AM_HAL_SYSCTRL_SLEEP_DEEP,
  AM_HAL_SYSCTRL_SLEEP_DEEPER
} am_hal_sysctrl_sleep_type_e;

#define AM_HAL_SYSCTRL_SLEEP_DEEPMAX    AM_HAL_SYSCTRL_SLEEP_DEEPER
//! @}

//*****************************************************************************
//
//! Definition of Global Power State enumeration
//
//*****************************************************************************
typedef enum
{
  AM_HAL_SYSCTRL_WAKE,
  AM_HAL_SYSCTRL_NORMALSLEEP,
  AM_HAL_SYSCTRL_DEEPSLEEP
} am_hal_sysctrl_power_state_e;

#define SYNC_READ       0x47FF0000

//*****************************************************************************
//
//! Write flush - This function will hold the bus until all queued write
//! operations on System Bus have completed, thereby guaranteeing that all
//! writes to APB have been flushed.
//
//*****************************************************************************
#define am_hal_sysctrl_sysbus_write_flush()     AM_REGVAL(SYNC_READ)

//*****************************************************************************
//
//! Write flush - This function will return once all queued write
//! operations have completed, thereby guaranteeing that all
//! writes have been flushed.
//! This works across all the buses - AXI and APB
//
//*****************************************************************************
#define am_hal_sysctrl_bus_write_flush()        am_hal_cachectrl_dcache_invalidate(NULL, true)

//*****************************************************************************
//
// Global Variables
//
//*****************************************************************************
extern bool g_bFrcBuckAct;

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
//*****************************************************************************
//
//! @brief Place the core into sleep, deepsleep or deepersleep.
//!
//! @param eSleepType - Normal or Deep, Deeper sleep.
//!
//! This function puts the MCU to sleep, deepsleep or deepersleep depending on eSleepType.
//!
//! Valid values for eSleepType are:
//!
//!     AM_HAL_SYSCTRL_SLEEP_NORMAL
//!     AM_HAL_SYSCTRL_SLEEP_DEEP
//!     AM_HAL_SYSCTRL_SLEEP_DEEPER
//
//*****************************************************************************
extern void am_hal_sysctrl_sleep(am_hal_sysctrl_sleep_type_e eSleepType);

//*****************************************************************************
//
//! @brief Control the buck state in deepsleep
//!
//! @param bFrcBuckAct - True for forcing buck active in deepsleep
//!                    - False for not forcing buck active in deepsleep
//!
//! If you want to manually force the buck stay active in deepsleep mode,
//! am_hal_sysctrl_force_buck_active_in_deepsleep must
//! be called for setting g_bAppFrcBuckAct to true before
//! calling am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP).
//! If anyone of spotmgr and
//! am_hal_sysctrl_force_buck_active_in_deepsleep forced buck stay active, buck
//! will stay active in deepsleep.
//
//*****************************************************************************
extern void am_hal_sysctrl_force_buck_active_in_deepsleep(bool bFrcBuckAct);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SYSCTRL_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

