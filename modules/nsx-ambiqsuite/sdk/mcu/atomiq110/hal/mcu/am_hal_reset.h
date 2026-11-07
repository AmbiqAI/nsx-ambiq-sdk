//*****************************************************************************
//
//! @file am_hal_reset.h
//!
//! @brief Hardware abstraction layer for the Reset Generator module.
//!
//! @addtogroup rstgen2_at110 Reset - Reset Generator (RSTGEN)
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
#ifndef AM_HAL_RSTGEN_H
#define AM_HAL_RSTGEN_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// RESET specific definitions.
//
//*****************************************************************************
//**************************************
//
//! Reset Generator configuration values
//
//**************************************
typedef enum
{
    // Enables
    AM_HAL_RESET_WDT_RESET_ENABLE,      // [1:1]
    AM_HAL_RESET_SECURE_WDT_ENABLE,     // [2:2]
    // Disables
    AM_HAL_RESET_WDT_RESET_DISABLE,     // [1:1]
    AM_HAL_RESET_SECURE_WDT_DISABLE     // [2:2]
} am_hal_reset_configure_e;

//**************************************
//
//! Reset Generator control operations
//
//**************************************
typedef enum
{
    AM_HAL_RESET_CONTROL_SWPOR,
    AM_HAL_RESET_CONTROL_SWPOI,
} am_hal_reset_control_e;

//**************************************
//
//! Reset Status
//
//**************************************
typedef enum
{
    AM_HAL_RESET_STATUS_EXTERNAL        = RSTGEN_STAT_EXRSTAT_Msk,          //  [0] Short External Reset (POR)
    AM_HAL_RESET_STATUS_POA             = RSTGEN_STAT_POASTAT_Msk,          //  [1] Power On Analog Reset (POA)
    AM_HAL_RESET_STATUS_BOR             = RSTGEN_STAT_BORSTAT_Msk,          //  [2] Brown-Out Reset
    AM_HAL_RESET_STATUS_SWPOR           = RSTGEN_STAT_SWRSTAT_Msk,          //  [3] Software Power On Reset (SWPOR)
    AM_HAL_RESET_STATUS_SWPOI           = RSTGEN_STAT_POIRSTAT_Msk,         //  [4] Software Power On Initialize (SWPOI)
    AM_HAL_RESET_STATUS_DEBUGGER        = RSTGEN_STAT_DBGRSTAT_Msk,         //  [5] Debugger Reset
    AM_HAL_RESET_STATUS_WDT             = RSTGEN_STAT_WDRSTAT_Msk,          //  [6] Watchdog Timer Reset
    AM_HAL_RESET_STATUS_BO18STAT        = RSTGEN_STAT_BO18STAT_Msk,         //  [7] Brownout 1.8v unregulated supply
    AM_HAL_RESET_STATUS_BOSSTAT         = RSTGEN_STAT_BOSSTAT_Msk,          //  [8] VDDS Analog Brownout Event
    AM_HAL_RESET_STATUS_BOFHSTAT        = RSTGEN_STAT_BOFHSTAT_Msk,         //  [9] VDDFH Analog Brownout Event
    AM_HAL_RESET_STATUS_BOFSTAT         = RSTGEN_STAT_BOFSTAT_Msk,          // [10] VDDF Analog Brownout Event
    AM_HAL_RESET_STATUS_BOCULVSTAT      = RSTGEN_STAT_BOCULVSTAT_Msk,       // [11] VDDC_ULV Analog Brownout Event
    AM_HAL_RESET_STATUS_BOCLVSTAT       = RSTGEN_STAT_BOCLVSTAT_Msk,        // [12] VDDC_LV Analog Brownout Event
    AM_HAL_RESET_STATUS_BOCSTAT         = RSTGEN_STAT_BOCSTAT_Msk,          // [13] VDDC Analog Brownout Event
    AM_HAL_RESET_STATUS_AIRCRSTAT       = RSTGEN_STAT_AIRCRSTAT_Msk,        // [14] System reset request (CM55 SYSRESETREQ)
    AM_HAL_RESET_STATUS_SECUREWDRSTAT   = RSTGEN_STAT_SECUREWDRSTAT_Msk,    // [15] Secure watchdog Timer Reset
    AM_HAL_RESET_STATUS_CM4TIMEOUTSTAT  = RSTGEN_STAT_CM4TIMEOUTSTAT_Msk,   // [16] Unused
    AM_HAL_RESET_STATUS_EXRLSTAT        = RSTGEN_STAT_EXRLSTAT_Msk,         // [17] Long External Reset (POI)
    AM_HAL_RESET_STATUS_RETSTAT         = RSTGEN_STAT_RETSTAT_Msk,          // [18] Retention Status, RW
    AM_HAL_RESET_STATUS_BO22STAT        = RSTGEN_STAT_BO22STAT_Msk,         // [19] Unused
    AM_HAL_RESET_STATUS_BORRAMSTAT      = RSTGEN_STAT_BORRAMSTAT_Msk,       // [20] RRAM brownout
} am_hal_reset_status_e;


#define AM_HAL_RESET_STATUS_MASK    (                                                           \
        RSTGEN_STAT_EXRSTAT_Msk         |   \
        RSTGEN_STAT_POASTAT_Msk         |   \
        RSTGEN_STAT_BORSTAT_Msk         |   \
        RSTGEN_STAT_SWRSTAT_Msk         |   \
        RSTGEN_STAT_POIRSTAT_Msk        |   \
        RSTGEN_STAT_DBGRSTAT_Msk        |   \
        RSTGEN_STAT_WDRSTAT_Msk         |   \
        RSTGEN_STAT_BO18STAT_Msk        |   \
        RSTGEN_STAT_BOSSTAT_Msk         |   \
        RSTGEN_STAT_BOFHSTAT_Msk        |   \
        RSTGEN_STAT_BOFSTAT_Msk         |   \
        RSTGEN_STAT_BOCULVSTAT_Msk      |   \
        RSTGEN_STAT_BOCLVSTAT_Msk       |   \
        RSTGEN_STAT_BOCSTAT_Msk         |   \
        RSTGEN_STAT_AIRCRSTAT_Msk       |   \
        RSTGEN_STAT_SECUREWDRSTAT_Msk   |   \
        RSTGEN_STAT_CM4TIMEOUTSTAT_Msk  |   \
        RSTGEN_STAT_EXRLSTAT_Msk        |   \
        RSTGEN_STAT_RETSTAT_Msk         |   \
        RSTGEN_STAT_BO22STAT_Msk        |   \
        RSTGEN_STAT_BORRAMSTAT_Msk )

//**************************************
//
//! RESET status structure
//
//**************************************
typedef struct
{
    am_hal_reset_status_e
            eStatus;            // Return all status bits from RSTGEN.STAT
    bool    bEXTStat;           // Short External Reset (POR)
    bool    bPOAStat;           // Power On Analog Reset (POA)
    bool    bBORStat;           // Brown-Out Reset
    bool    bSWPORStat;         // Software Power On Reset (SWPOR)
    bool    bSWPOIStat;         // Software Power On Initialize (SWPOI)
    bool    bDBGRStat;          // Debugger Reset
    bool    bWDTStat;           // Watchdog Timer Reset
    bool    bBO18Stat;          // Brownout 1.8v unregulated supply
    bool    bBOSStat;           // VDDS Analog Brownout Event
    bool    bBOFHStat;          // VDDFH Analog Brownout Event
    bool    bBOFStat;           // VDDF Analog Brownout Event
    bool    bBOCULVStat;        // VDDC_ULV Analog Brownout Event
    bool    bBOCLVStat;         // VDDC_LV Analog Brownout Event
    bool    bBOCStat;           // VDDC Analog Brownout Event
    bool    bAIRCRStat;         // System reset request (CM55 SYSRESETREQ)
    bool    bSecureWdtRst;      // Secure watchdog Timer Reset
    bool    bCM4TimeoutStat;    // Unused
    bool    bEXRLStat;          // Long External Reset (POI)
    bool    bRETStat;           // Retention Status, RW
    bool    bBO22Stat;          // Unused
    bool    bBORRAMStat;        // RRAM brownout
} am_hal_reset_status_t;

//
//! Define interrupt bit(s)
//
#define AM_HAL_RESET_INTERRUPT_BODH     RSTGEN_INTEN_BODDIGS_Msk


//
// Global variable used to capture the reset status
//
extern uint32_t gAmHalResetStatus;

//*****************************************************************************
//
//! @brief Enable and configure the Reset controller.
//!
//! This function will configure the specified reset conditions.
//!
//! @param eConfigure - One of configuration enumerations.
//!     AM_HAL_RESET_WDT_RESET_ENABLE
//!     AM_HAL_RESET_WDT_RESET_DISABLE
//!
//! @return status - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_reset_configure(am_hal_reset_configure_e eConfigure);

//*****************************************************************************
//
//! @brief Reset generator control function.
//!
//! This function will perform various reset functions including assertion
//! of software resets.
//!
//! @param eControl - One of the control enumerations.
//!     AM_HAL_RESET_CONTROL_SWPOR - power on reset, which results in a reset of
//!         all blocks except for registers in clock gen, RTC, stimer, PMU.
//!         Equivalent to the reset state obtained by a hardware reset, use of
//!         the ARM AIRCR (Application Interrupt and Reset Control Register)
//!         core register, debugger reset, watchdog timer expiration, or
//!         brown-out event.
//!     AM_HAL_RESET_CONTROL_SWPOI - power on initialization, which results in a
//!         reset of all blocks except for registers in clock gen, RTC, stimer.
//!         The POI reset level is required in order to enable configuration
//!         changes such as memory protection.
//! @param pArgs - Pointer to arguments for Control Switch Case
//!
//! @return status - generic or interface specific status.
//! When resetting the chip (SWPOR or SWPOI), the function will obviously
//! not return to the caller.
//
//*****************************************************************************
extern uint32_t am_hal_reset_control(am_hal_reset_control_e eControl,
                                     void *pArgs);

//*****************************************************************************
//
//! @brief Return status of the reset generator.
//!
//! This function will get the status bits from the reset generator.
//! The status value shows the type of reset(s) that have occurred since power
//! on
//! Application MUST call this API at least once before going to deepsleep
//! Otherwise this API will not provide correct reset status
//!
//! @param psStatus - Pointer to a data structure to receive the status
//! information. Most members of the structure are booleans that receive
//! the status of a particular bit.
//!
//! The eStatus member, however, returns a bitmask of one or more of the
//! following values:
//!     AM_HAL_RESET_STATUS_EXTERNAL
//!     AM_HAL_RESET_STATUS_POA
//!     AM_HAL_RESET_STATUS_BOD
//!     AM_HAL_RESET_STATUS_SWPOR
//!     AM_HAL_RESET_STATUS_SWPOI
//!     AM_HAL_RESET_STATUS_DEBUGGER
//!     AM_HAL_RESET_STATUS_WDT
//!     AM_HAL_RESET_STATUS_BOUNREG
//!     AM_HAL_RESET_STATUS_BOCORE
//!     AM_HAL_RESET_STATUS_BOMEM
//!     AM_HAL_RESET_STATUS_BOHPMEM
//!     AM_HAL_RESET_STATUS_BOLPCORE
//!
//! @return status. If the API was never called before a valid reset status
//! could be captured, AM_HAL_STATUS_FAIL is returned.
//! Otherwise AM_HAL_STATUS_SUCCESS implies valid reset status returned
//
//*****************************************************************************
extern uint32_t am_hal_reset_status_get(am_hal_reset_status_t *psStatus);

//*****************************************************************************
//
//! @brief Enable selected RSTGEN Interrupts.
//!
//! Use this function to enable the interrupts.
//!
//! @param ui32IntMask - One or more of the following bits, any of which can
//! be ORed together.
//!   AM_HAL_RESET_INTERRUPT_BODH
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_reset_interrupt_enable(uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Disable selected RSTGEN Interrupts.
//!
//! Use this function to disable the RSTGEN interrupts.
//!
//! @param ui32IntMask - One or more of the following bits, any of which can
//! be ORed together.
//!   AM_HAL_RESET_INTERRUPT_BODH
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_reset_interrupt_disable(uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Reset generator interrupt clear
//!
//! This function clears the reset generator interrupts.
//!
//! @param ui32IntMask - One or more of the following bits, any of which can
//! be ORed together.
//!   AM_HAL_RESET_INTERRUPT_BODH
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_reset_interrupt_clear(uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Get interrupt status of reset generator.
//!
//! This function returns the interrupt status for the reset generator.
//!
//! @param bEnabledOnly determines whether disabled interrupts are included in
//! the status.
//! @param pui32IntStatus - ptr to uint32_t to return the interrupt status.
//!
//! The following are valid status bits.
//!   AM_HAL_RESET_INTERRUPT_BODH
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_reset_interrupt_status_get(bool bEnabledOnly,
                                                  uint32_t *pui32IntStatus);


#ifdef __cplusplus
}
#endif

#endif // AM_HAL_RSTGEN_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

