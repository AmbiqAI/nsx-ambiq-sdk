//*****************************************************************************
//
//! @file am_hal_rtc.h
//!
//! @brief Functions for interfacing with the Real-Time Clock (RTC).
//!
//! @addtogroup rtc4_at110 RTC - Real-Time Clock
//! @ingroup atomiq110_hal
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
#ifndef AM_HAL_RTC_H
#define AM_HAL_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name Oscillator Selector.
//! @{
//! Options for the RTC oscillator.
//!
//! Used with am_hal_rtc_config
//
//*****************************************************************************
typedef enum
{
    AM_HAL_RTC_OSC_XT   = 0x0,
    AM_HAL_RTC_OSC_LFRC = 0x1,
}
am_hal_rtc_osc_select_e;
//! @}

//*****************************************************************************
//
//! @name RTC Interrupts
//! @{
//! Macro definitions for RTC interrupt status bits.
//!
//! These macros correspond to the bits in the RTC interrupt status register.
//! They may be used with any of the \e am_hal_rtc_interrupt_x() functions.
//!
//
//*****************************************************************************
#define AM_HAL_RTC_INT_ALM                  RTC_INTEN_ALM_Msk
//! @}

// #### INTERNAL BEGIN ####
//*****************************************************************************
//
//! @name Reserved timer for RTC work-around
//! @{
//! The RTC counter w/a needs a timer to poll the clock edge,
//!
//! either the reserved timer by  RTC_CDC_WA_USE_APP_STMR = 0 (default),
//! or stimer from application by RTC_CDC_WA_USE_APP_STMR = 1 (XT for RTC only)
//!   app stimer codes from Milton, and being verified with customer setups
//!   https://ambiqmicro.atlassian.net/browse/COR-138?focusedCommentId=175558
//
//*****************************************************************************
#ifdef RTC_READ_WA
#define RTC_CDC_WA_USE_APP_STMR 0
#endif
//! @}
// #### INTERNAL END ####

//*****************************************************************************
//
//! @name RTC Alarm Repeat Interval.
//! @{
//! Macro definitions for the RTC alarm repeat interval.
//!
//! These macros correspond to the RPT bits in the RTCCTL register.
//! They may be used with the \e am_hal_rtc_alarm_interval_set() function.
//!
//! Note: AM_HAL_RTC_ALM_RPT_10TH and AM_HAL_RTC_ALM_RPT_100TH do not
//! correspond to the RPT bits but are used in conjunction with setting the
//! ALM100 bits in the ALMLOW register.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_RTC_ALM_RPT_DIS = RTC_RTCCTL_RPT_DIS,
    AM_HAL_RTC_ALM_RPT_YR  = RTC_RTCCTL_RPT_YEAR,
    AM_HAL_RTC_ALM_RPT_MTH = RTC_RTCCTL_RPT_MONTH,
    AM_HAL_RTC_ALM_RPT_WK  = RTC_RTCCTL_RPT_WEEK,
    AM_HAL_RTC_ALM_RPT_DAY = RTC_RTCCTL_RPT_DAY,
    AM_HAL_RTC_ALM_RPT_HR  = RTC_RTCCTL_RPT_HR,
    AM_HAL_RTC_ALM_RPT_MIN = RTC_RTCCTL_RPT_MIN,
    AM_HAL_RTC_ALM_RPT_SEC = RTC_RTCCTL_RPT_SEC,
    AM_HAL_RTC_ALM_RPT_10TH,
    AM_HAL_RTC_ALM_RPT_100TH,
}
am_hal_rtc_alarm_repeat_e;
//! @}

//*****************************************************************************
//
//! RTC configuration structure.
//!
//! @note The HR1224 (12/24-hour) feature has been removed. \e b12Hour is
//! retained for API compatibility but is ignored by am_hal_rtc_config().
//
//*****************************************************************************
typedef struct am_hal_rtc_config_struct
{
    //! Oscillator source for the RTC.
    am_hal_rtc_osc_select_e eOscillator;
    //! Unused. Retained for API compatibility; HR1224 has been removed.
    bool b12Hour;
}
am_hal_rtc_config_t;

//*****************************************************************************
//
//! @brief The basic time structure used by the HAL for RTC interaction.
//!
//! All values are positive whole numbers. The HAL routines convert back and
//! forth to BCD.
//
//*****************************************************************************
typedef struct am_hal_rtc_time_struct
{
    uint32_t ui32ReadError;
    uint32_t ui32Weekday;
    uint32_t ui32CenturyBit;
    uint32_t ui32Year;
    uint32_t ui32Month;
    uint32_t ui32DayOfMonth;
    uint32_t ui32Hour;
    uint32_t ui32Minute;
    uint32_t ui32Second;
    uint32_t ui32Hundredths;
}
am_hal_rtc_time_t;

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Configures the RTC
//!
//! @param psConfig - pointer to configuration structure for RTC.
//!
//! Configures the oscillator for the RTC. See the \e am_hal_rtc_config_t
//! structure for more information.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_config(const am_hal_rtc_config_t *psConfig);

//*****************************************************************************
//
//! @brief Enable/Start the RTC oscillator.
//!
//! Starts the RTC oscillator.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_osc_enable(void);

//*****************************************************************************
//
//! @brief Disable/Stop the RTC oscillator.
//!
//! Stops the RTC oscillator.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_osc_disable(void);

//*****************************************************************************
//
//! @brief Set the Real Time Clock counter registers.
//!
//! @param pTime - pointer to time structure.
//!
//! Sets the RTC counter registers to the supplied values.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_time_set(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Get the Real Time Clock current time.
//!
//! @param pTime - pointer to time structure to store current time.
//!
//! Gets the RTC's current time.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_time_get(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Sets the RTC's Alarm.
//!
//! @param pTime - pointer to time structure.
//! @param eRepeatInterval - desired alarm repeat interval.
//!
//! Set the Real Time Clock Alarm Parameters.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_alarm_set(am_hal_rtc_time_t *pTime,
                                     am_hal_rtc_alarm_repeat_e eRepeatInterval);

//*****************************************************************************
//
//! @brief Get the Real Time Clock Alarm Parameters
//!
//! @param pTime - pointer to time structure to store current alarm.
//! @param peRepeatInterval - pointer to store current alarm repeat interval.
//!
//! Retrieves the RTC's current alarm time and repeat interval settings.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_alarm_get(am_hal_rtc_time_t *pTime,
                                     am_hal_rtc_alarm_repeat_e *peRepeatInterval);

//*****************************************************************************
//
//! @brief Enable selected RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Enables the RTC interrupts. \e ui32InterruptMask should be set to a
//! logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_enable(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Return the enabled RTC interrupts.
//!
//! @param pui32InterruptMask - pointer where current RTC interrupt
//!        enable mask will be written.
//!
//! Returns the enabled RTC interrupts. \e pui32InterruptMask will be set to a
//! logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//!
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_enable_get(uint32_t *pui32InterruptMask);

//*****************************************************************************
//
//! @brief Disable selected RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Disables the RTC interrupts. \e ui32InterruptMask should be set to a
//! logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_disable(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Clear selected RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Clears the RTC interrupts. \e ui32InterruptMask should be set to a
//! logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_clear(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Sets the selected RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Sets the RTC interrupts causing them to immediately trigger.
//! \e ui32InterruptMask should be set to a logical OR of one or more of
//! the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_set(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Returns the RTC interrupt status.
//!
//! @param bEnabledOnly - if true, return status of enabled interrupts only.
//! @param pui32InterruptMask - pointer where interrupt status is written.
//!
//! This function will write the current RTC interrupt status to \e
//! pui32InterruptMask. The interrupt status value will be the logical OR of
//! one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_interrupt_status_get(bool bEnabledOnly,
                                                uint32_t *pui32InterruptMask);

//*****************************************************************************
//
//! @brief Selects the clock source for the RTC.
//!
//! @param ui32OSC - clock source for RTC.
//!
//! This function selects the clock source for the RTC.
//!
//! Valid values for ui32OSC are:
//!
//!     AM_HAL_RTC_OSC_LFRC
//!     AM_HAL_RTC_OSC_XT
//!
//*****************************************************************************
extern void am_hal_rtc_osc_select(uint32_t ui32OSC);

//*****************************************************************************
//
//! @brief Returns the clock source selected for the RTC.
//!
//! This function returns the RTC oscillator selection from
//! CLKGEN.OCTRL.RTCOSEL.
//!
//! @param pui32OSC - pointer to variable for query result. It will
//!                    be written to one of the following:
//!                    - AM_HAL_RTC_OSC_LFRC
//!                    - AM_HAL_RTC_OSC_XT
//!
//! @note This reflects the configured selection in CLKGEN.OCTRL.RTCOSEL
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_rtc_osc_get(uint32_t *pui32OSC);

//*****************************************************************************
//
//! @brief Sets the alarm repeat interval.
//!
//! @param ui32RepeatInterval - desired repeat interval.
//!
//! Sets the alarm repeat interval.
//!
//! Valid values for ui32RepeatInterval:
//!
//!     AM_HAL_RTC_ALM_RPT_DIS
//!     AM_HAL_RTC_ALM_RPT_YR
//!     AM_HAL_RTC_ALM_RPT_MTH
//!     AM_HAL_RTC_ALM_RPT_WK
//!     AM_HAL_RTC_ALM_RPT_DAY
//!     AM_HAL_RTC_ALM_RPT_HR
//!     AM_HAL_RTC_ALM_RPT_MIN
//!     AM_HAL_RTC_ALM_RPT_SEC
//!     AM_HAL_RTC_ALM_RPT_10TH
//!     AM_HAL_RTC_ALM_RPT_100TH
//!
//
//*****************************************************************************
extern void am_hal_rtc_alarm_interval_set(uint32_t ui32RepeatInterval);

//*****************************************************************************
//
//! @brief Clears the RTC counter.
//
//*****************************************************************************
extern void am_hal_rtc_counter_clear(void);

// #### INTERNAL BEGIN ####

//*****************************************************************************
//
//! @brief Configures the Secure RTC
//!
//! @param psConfig - pointer to configuration structure for Secure RTC.
//!
//! Configures the oscillator for the Secure RTC. See the
//! \e am_hal_rtc_config_t structure for more information.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_config(const am_hal_rtc_config_t *psConfig);

//*****************************************************************************
//
//! @brief Enable/Start the Secure RTC oscillator.
//!
//! Starts the Secure RTC oscillator.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_osc_enable(void);

//*****************************************************************************
//
//! @brief Disable/Stop the Secure RTC oscillator.
//!
//! Stops the Secure RTC oscillator.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_osc_disable(void);

//*****************************************************************************
//
//! @brief Set the Secure Real Time Clock counter registers.
//!
//! @param pTime - pointer to time structure.
//!
//! Sets the Secure RTC counter registers to the supplied values.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_time_set(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Get the Secure Real Time Clock current time.
//!
//! @param pTime - pointer to time structure to store current time.
//!
//! Gets the Secure RTC's current time.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_time_get(am_hal_rtc_time_t *pTime);

//*****************************************************************************
//
//! @brief Sets the Secure RTC's Alarm.
//!
//! @param pTime - pointer to time structure.
//! @param eRepeatInterval - desired alarm repeat interval.
//!
//! Set the Secure Real Time Clock Alarm Parameters.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_alarm_set(am_hal_rtc_time_t *pTime,
                                            am_hal_rtc_alarm_repeat_e eRepeatInterval);

//*****************************************************************************
//
//! @brief Get the Secure Real Time Clock Alarm Parameters
//!
//! @param pTime - pointer to time structure to store current alarm.
//! @param peRepeatInterval - pointer to store current alarm repeat interval.
//!
//! Retrieves the Secure RTC's current alarm time and repeat interval settings.
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_alarm_get(am_hal_rtc_time_t *pTime,
                                            am_hal_rtc_alarm_repeat_e *peRepeatInterval);

//*****************************************************************************
//
//! @brief Enable selected Secure RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Enables the Secure RTC interrupts. \e ui32InterruptMask should be
//! set to a logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_enable(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Return the enabled Secure RTC interrupts.
//!
//! @param pui32InterruptMask - pointer where current Secure RTC
//!        interrupt enable mask will be written.
//!
//! Returns the enabled Secure RTC interrupts.
//! \e pui32InterruptMask will be set to a logical OR of one or more
//! of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_enable_get(uint32_t *pui32InterruptMask);

//*****************************************************************************
//
//! @brief Disable selected Secure RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Disables the Secure RTC interrupts. \e ui32InterruptMask should be
//! set to a logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_disable(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Clear selected Secure RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Clears the Secure RTC interrupts. \e ui32InterruptMask should be set
//! to a logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_clear(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Sets the selected Secure RTC interrupts.
//!
//! @param ui32InterruptMask - desired interrupts
//!
//! Sets the Secure RTC interrupts causing them to immediately trigger.
//! \e ui32InterruptMask should be set to a logical OR of one or more of
//! the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_set(uint32_t ui32InterruptMask);

//*****************************************************************************
//
//! @brief Returns the Secure RTC interrupt status.
//!
//! @param bEnabledOnly - if true, return status of enabled interrupts only.
//! @param pui32InterruptMask - pointer where interrupt status is written.
//!
//! This function will write the current Secure RTC interrupt status
//! to \e pui32InterruptMask. The interrupt status value will be the
//! logical OR of one or more of the following:
//!
//!     AM_HAL_RTC_INT_ALM
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_interrupt_status_get(bool bEnabledOnly,
                                                       uint32_t *pui32InterruptMask);

//*****************************************************************************
//
//! @brief Selects the clock source for the Secure RTC.
//!
//! @param ui32OSC - clock source for Secure RTC.
//!
//! This function selects the clock source for the Secure RTC.
//!
//! Valid values for ui32OSC are:
//!
//!     AM_HAL_RTC_OSC_LFRC
//!     AM_HAL_RTC_OSC_XT
//
//*****************************************************************************
extern void am_hal_secure_rtc_osc_select(uint32_t ui32OSC);

//*****************************************************************************
//
//! @brief Returns the clock source selected for the Secure RTC.
//!
//! This function returns the Secure RTC oscillator selection from
//! CLKGEN.OCTRL.SECURERTCOSEL.
//!
//! @param pui32OSC - pointer to variable for query result. It will
//!                    be written to one of the following:
//!                    - AM_HAL_RTC_OSC_LFRC
//!                    - AM_HAL_RTC_OSC_XT
//!
//! @note This reflects the configured selection in
//!       CLKGEN.OCTRL.SECURERTCOSEL
//!
//! @return AM_HAL_STATUS_SUCCESS or relevant HAL error code.
//
//*****************************************************************************
extern uint32_t am_hal_secure_rtc_osc_get(uint32_t *pui32OSC);

//*****************************************************************************
//
//! @brief Sets the Secure RTC alarm repeat interval.
//!
//! @param ui32RepeatInterval - desired repeat interval.
//!
//! Sets the Secure RTC alarm repeat interval.
//!
//! Valid values for ui32RepeatInterval:
//!
//!     AM_HAL_RTC_ALM_RPT_DIS
//!     AM_HAL_RTC_ALM_RPT_YR
//!     AM_HAL_RTC_ALM_RPT_MTH
//!     AM_HAL_RTC_ALM_RPT_WK
//!     AM_HAL_RTC_ALM_RPT_DAY
//!     AM_HAL_RTC_ALM_RPT_HR
//!     AM_HAL_RTC_ALM_RPT_MIN
//!     AM_HAL_RTC_ALM_RPT_SEC
//!     AM_HAL_RTC_ALM_RPT_10TH
//!     AM_HAL_RTC_ALM_RPT_100TH
//!
//
//*****************************************************************************
extern void am_hal_secure_rtc_alarm_interval_set(uint32_t ui32RepeatInterval);

//*****************************************************************************
//
//! @brief Clears the Secure RTC counter.
//
//*****************************************************************************
extern void am_hal_secure_rtc_counter_clear(void);

// #### INTERNAL END ####
#ifdef __cplusplus
}
#endif

#endif // AM_HAL_RTC_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

