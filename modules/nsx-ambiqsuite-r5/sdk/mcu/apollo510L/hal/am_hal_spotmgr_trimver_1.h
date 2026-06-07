//*****************************************************************************
//
//! @file am_hal_spotmgr_trimver_1.h
//!
//! @brief SPOT Manager Trim Version 1 Implementation.
//!
//! @addtogroup spotmgr_trim1_ap510L SPOT Manager Trim Version 1
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
#ifndef AM_HAL_SPOTMGR_TRIMVER_1_H
#define AM_HAL_SPOTMGR_TRIMVER_1_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! Defines
//
//*****************************************************************************
// #### INTERNAL BEGIN ####
//
// The CORELDOACTIVETRIM trim code reduced in simobuck init
//
// #define CORELDOACTIVETRIM_REDUCE_IN_SIMOBUCK_INIT  7
// #### INTERNAL END ####

//*****************************************************************************
//
//! Globals
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Power states update
//!
//! @param eStimulus - Stimilus for power states transition. For GPU state/power
//!                    changes, please use AM_HAL_SPOTMGR_STIM_GPU_STATE but not
//!                    AM_HAL_SPOTMGR_STIM_DEVPWR.
//! @param bOn       - Only needs to be set to true/false when turning on/off
//!                    peripherals or memories included in DEVPWRSTATUS,
//!                    AUDSSPWRSTATUS, MEMPWRSTATUS and SSRAMPWRST. It is
//!                    ignored when updating temperature, CPU state and GPU state.
//! @param pArgs     - Pointer to arguments for power states update, assign it
//!                    to NULL if not needed.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_CPU_STATE,
//! bOn is ignored, and pArgs must point to a am_hal_spotmgr_cpu_state_e enum.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_GPU_STATE,
//! bOn is ignored, and pArgs must point to a am_hal_spotmgr_gpu_state_e enum.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_TEMP,
//! bOn is ignored, and pArgs must point to a am_hal_spotmgr_tempco_param_t struct.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_DEVPWR,
//! bOn must be set to true when turning on a peripheral,
//! bOn must be set to false when turning off a peripheral,
//! and pArgs must point to the DEVPWRSTATUS_MASK of the peripheral to be opened
//! when turning on a peripheral, pArgs is ignored when turning off a peripheral.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_AUDSSPWR,
//! bOn must be set to true when turning on a peripheral,
//! bOn must be set to false when turning off a peripheral,
//! and pArgs must point to the AUDSSPWRSTATUS_MASK of the peripheral to be opened
//! when turning on a peripheral, pArgs is ignored when turning off a peripheral.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_MEMPWR,
//! bOn is ignored, and pArgs must point to the entire MEMPWRSTATUS.
//!
//! When eStimulus is AM_HAL_SPOTMGR_STIM_SSRAMPWR,
//! bOn must be set to true when turning on partial or entire SSRAM,
//! bOn must be set to false when turning off partial or entire SSRAM,
//! and pArgs must point to the expected SSRAMPWRST when turning on,
//! pArgs is ignored when turning off.
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_power_state_update(am_hal_spotmgr_stimulus_e eStimulus, bool bOn, void *pArgs);

//*****************************************************************************
//
//! @brief Reset power state to POR default
//!        Do not report a fixed temperature here, user must report temperature
//!        after transioning to a new image to make sure SIMOBUCK state is
//!        correct in deep sleep or deeper sleep.
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_default_reset(void);

//*****************************************************************************
//
//! @brief SPOT manager init
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_init(void);

// #### INTERNAL BEGIN ####
#if 0
//*****************************************************************************
//
//! @brief SIMOBUCK initialziation handling at stage just before enabling
//!        SIMOBUCK
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_simobuck_init_bfr_enable(void);

//*****************************************************************************
//
//! @brief SIMOBUCK initialziation handling at stage just after enabling
//!        SIMOBUCK
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_simobuck_init_aft_enable(void);
#endif
// #### INTERNAL END ####

#if NO_TEMPSENSE_IN_DEEPSLEEP
//*****************************************************************************
//
//! @brief Prepare SPOT manager for suspended tempco during deep sleep
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_trimver_1_tempco_suspend(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SPOTMGR_TRIMVER_1_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************


