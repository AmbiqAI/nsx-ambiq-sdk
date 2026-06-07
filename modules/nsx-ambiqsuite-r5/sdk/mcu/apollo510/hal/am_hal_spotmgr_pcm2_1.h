//*****************************************************************************
//
//! @file am_hal_spotmgr_pcm2_1.h
//!
//! @brief SPOT manager functions that manage power states for PCM2.1 parts
//!
//! @addtogroup spotmgr_21_ap510 SPOTMGR - SPOT Manager PCM2.1
//! @ingroup apollo510_hal
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
#ifndef AM_HAL_SPOTMGR_PCM2_1_H
#define AM_HAL_SPOTMGR_PCM2_1_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! Configurations
//
//*****************************************************************************
//! Control whether SPOTMGR delay uses TIMER (true) or blocking delay (false)
#define AM_HAL_SPOTMGR_TIMER_DELAY_PCM2_1 AM_HAL_SPOTMANAGER_USE_TIMER_OR_BLOCKING_DELAY
//! Control whether SPOTMGR uses double boost mode for transitions using timer delay
#define AM_HAL_SPOTMGR_DOUBLE_BOOST_PCM2_1 AM_HAL_SPOTMANAGER_TIMER_DOUBLEBOOST

//*****************************************************************************
//
//! @brief Power states update for PCM2.1
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
extern uint32_t am_hal_spotmgr_pcm2_1_power_state_update(am_hal_spotmgr_stimulus_e eStimulus, bool bOn, void *pArgs);

//*****************************************************************************
//
//! @brief Reset power state to POR default for PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_default_reset(void);

//*****************************************************************************
//
//! @brief SPOT manager init for PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_init(void);

//*****************************************************************************
//
//! @brief SIMOBUCK initialziation handling at stage just before overriding
//!        LDO/SIMOBUCK for PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_simobuck_init_bfr_ovr(void);

//*****************************************************************************
//
//! @brief SIMOBUCK initialziation handling at stage just before enabling
//!        SIMOBUCK for PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_simobuck_init_bfr_enable(void);

//*****************************************************************************
//
//! @brief SIMOBUCK initialziation handling at stage just after enabling
//!        SIMOBUCK for PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_simobuck_init_aft_enable(void);

#if NO_TEMPSENSE_IN_DEEPSLEEP
//*****************************************************************************
//
//! @brief Prepare SPOT manager for suspended tempco during deep sleep for
//!        PCM2.1
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_tempco_suspend(void);
#endif

//*****************************************************************************
//
//! @brief Timer interrupt service for spotmgr
//!
//! @return None.
//
//*****************************************************************************
extern void am_hal_spotmgr_pcm2_1_boost_timer_interrupt_service(void);

//*****************************************************************************
//
//! @brief Power settings after CPU LP to HP transition for PCM2.1.
//!
//! @return SUCCESS or other Failures.
//
//*****************************************************************************
extern uint32_t am_hal_spotmgr_pcm2_1_post_lptohp_handle(void);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SPOTMGR_PCM2_1_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

