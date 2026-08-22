//*****************************************************************************
//
//! @file am_hal_clkgen.h
//!
//! @brief Functions for interfacing with the CLKGEN.
//!
//! @addtogroup clkgen4_at110 CLKGEN - Clock Generator
//! @ingroup atomiq110_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_CLKGEN_H
#define AM_HAL_CLKGEN_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @name System Clock max frequency
//! @{
//! Defines the maximum clock frequency for this device.
//! These macros provide a definition of the maximum clock frequency.
//
//*****************************************************************************
#ifdef ATOMIQ11X_FPGA
// TODO: check actual frequencies on FPGA
#define AM_HAL_CLKGEN_FREQ_ULP_HZ           (100 * 1000000)
#define AM_HAL_CLKGEN_FREQ_LP_HZ            (250 * 1000000)
#define AM_HAL_CLKGEN_FREQ_HP_HZ            (500 * 1000000)
#define AM_HAL_CLKGEN_FREQ_MAX_HZ           AM_HAL_CLKGEN_FREQ_HP_HZ
#define AM_HAL_CLKGEN_FREQ_HP192_HZ         (16 * 1000000)
#define AM_HAL_CLKGEN_FREQ_HP250_HZ         (20 * 1000000)
#else // ATOMIQ11X_FPGA
#define AM_HAL_CLKGEN_FREQ_ULP_HZ           (100 * 1000000)
#define AM_HAL_CLKGEN_FREQ_LP_HZ            (250 * 1000000)
#define AM_HAL_CLKGEN_FREQ_HP_HZ            (500 * 1000000)
// TODO: remove stale definitions of HP192 and HP250
#define AM_HAL_CLKGEN_FREQ_MAX_HZ           96000000
#define AM_HAL_CLKGEN_FREQ_HP192_HZ         192000000
#define AM_HAL_CLKGEN_FREQ_HP250_HZ         250000000
#endif // ATOMIQ11X_FPGA

#define AM_HAL_CLKGEN_FREQ_ULP_KHZ      (AM_HAL_CLKGEN_FREQ_ULP_HZ / 1000)
#define AM_HAL_CLKGEN_FREQ_LP_KHZ       (AM_HAL_CLKGEN_FREQ_LP_HZ  / 1000)
#define AM_HAL_CLKGEN_FREQ_HP_KHZ       (AM_HAL_CLKGEN_FREQ_HP_HZ  / 1000)
#define AM_HAL_CLKGEN_FREQ_ULP_MHZ      (AM_HAL_CLKGEN_FREQ_ULP_HZ / 1000000)
#define AM_HAL_CLKGEN_FREQ_LP_MHZ       (AM_HAL_CLKGEN_FREQ_LP_HZ  / 1000000)
#define AM_HAL_CLKGEN_FREQ_HP_MHZ       (AM_HAL_CLKGEN_FREQ_HP_HZ  / 1000000)

#define AM_HAL_CLKGEN_FREQ_MAX_KHZ      (AM_HAL_CLKGEN_FREQ_MAX_HZ / 1000)
#define AM_HAL_CLKGEN_FREQ_HP192_KHZ    (AM_HAL_CLKGEN_FREQ_HP192_HZ / 1000)
#define AM_HAL_CLKGEN_FREQ_HP250_KHZ    (AM_HAL_CLKGEN_FREQ_HP250_HZ / 1000)
#define AM_HAL_CLKGEN_FREQ_MAX_MHZ      (AM_HAL_CLKGEN_FREQ_MAX_HZ / 1000000)
#define AM_HAL_CLKGEN_FREQ_HP192_MHZ    (AM_HAL_CLKGEN_FREQ_HP192_HZ / 1000000)
#define AM_HAL_CLKGEN_FREQ_HP250_MHZ    (AM_HAL_CLKGEN_FREQ_HP250_HZ / 1000000)
#define AM_HAL_CLKGEN_CORESEL_MAXDIV    1
//! @}

//
//! Control operations.
//
typedef enum
{
    AM_HAL_CLKGEN_CONTROL_RTC_SEL_XTAL,
    AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC,
    // #### INTERNAL BEGIN ####
    AM_HAL_CLKGEN_CONTROL_SECURE_RTC_SEL_XTAL,
    AM_HAL_CLKGEN_CONTROL_SECURE_RTC_SEL_LFRC,
    // AM_HAL_CLKGEN_CONTROL_HFADJ_ENABLE,
    // AM_HAL_CLKGEN_CONTROL_HFADJ_DISABLE,
    // #### INTERNAL END ####
    AM_HAL_CLKGEN_CONTROL_I3C0CLKEN_ENABLE,
    AM_HAL_CLKGEN_CONTROL_I3C0CLKEN_DISABLE,
    AM_HAL_CLKGEN_CONTROL_I3C1CLKEN_ENABLE,
    AM_HAL_CLKGEN_CONTROL_I3C1CLKEN_DISABLE,
    AM_HAL_CLKGEN_CONTROL_I3C2CLKEN_ENABLE,
    AM_HAL_CLKGEN_CONTROL_I3C2CLKEN_DISABLE,
    // #### INTERNAL BEGIN ####
    // TODO: Confirm can be remove
    // AM_HAL_CLKGEN_CONTROL_PLLVCOEN_ENABLE,
    // AM_HAL_CLKGEN_CONTROL_PLLVCOEN_NORMALOP,
    AM_HAL_CLKGEN_CONTROL_PLLDIVEN_ENABLE,
    AM_HAL_CLKGEN_CONTROL_PLLDIVEN_NORMALOP,
    // #### INTERNAL END ####
    AM_HAL_CLKGEN_CONTROL_SELECT_EXTREF,
    AM_HAL_CLKGEN_CONTROL_SELECT_XT,
    // #### INTERNAL BEGIN ####
    // AM_HAL_CLKGEN_CONTROL_DISPCTRLCLK_ENABLE,
    // AM_HAL_CLKGEN_CONTROL_DISPCTRLCLK_DISABLE,
    // AM_HAL_CLKGEN_CONTROL_GFXCORECLK_ENABLE,
    // AM_HAL_CLKGEN_CONTROL_GFXCORECLK_DISABLE
    // #### INTERNAL END ####
} am_hal_clkgen_control_e;

//
//! Current RTC oscillator.
//
typedef enum
{
    AM_HAL_CLKGEN_STATUS_RTCOSC_XTAL,
    AM_HAL_CLKGEN_STATUS_RTCOSC_LFRC,
} am_hal_clkgen_status_rtcosc_e;

//
//! Clock Generation CLKOUT
//
// #### INTERNAL BEGIN ####
// TODO:
// The enum in atomiq110.h has conflict between its enum name and its
// description. As of now, it seems to be that the description is correct
// according to a peek into the RTL. However, we should verify this with FPGA
// when clock related logics are released, and understand the full picture.
// #### INTERNAL END ####
typedef enum
{
    // LFRC
    AM_HAL_CLKGEN_CLKOUT_LFRC           = CLKGEN_CLKOUT_CKSEL_LFRCDIV1,              // LFRC               ~ 32.768 KHz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV2      = CLKGEN_CLKOUT_CKSEL_LFRCDIV2,              // LFRC / 2           ~ 16.384 KHz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV32     = CLKGEN_CLKOUT_CKSEL_LFRCDIV32,             // LFRC / 32          ~ 1.024 KHz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV64     = CLKGEN_CLKOUT_CKSEL_LFRCDIV64,             // LFRC / 64          ~ 512 Hz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV512    = CLKGEN_CLKOUT_CKSEL_LFRCDIV512,            // LFRC / 512         ~ 64 Hz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV32K    = CLKGEN_CLKOUT_CKSEL_LFRCDIV32K,            // LFRC / 32768       ~ 1 Hz
    AM_HAL_CLKGEN_CLKOUT_LFRC_DIV1M     = CLKGEN_CLKOUT_CKSEL_LFRCDIV1M,             // LFRC / 1M          ~ 0.03125 Hz

    // XTAL
    AM_HAL_CLKGEN_CLKOUT_XTAL           = CLKGEN_CLKOUT_CKSEL_XT32KHZ,               // XTAL               = 32.768 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV2      = CLKGEN_CLKOUT_CKSEL_XT16KHZ,               // XTAL / 2           = 16.384 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV4      = CLKGEN_CLKOUT_CKSEL_XT8KHZ,                // XTAL / 4           = 8.192 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV8      = CLKGEN_CLKOUT_CKSEL_XT4KHZ,                // XTAL / 8           = 4.096 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV16     = CLKGEN_CLKOUT_CKSEL_XT2KHZ,                // XTAL / 16          = 2.048 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV32     = CLKGEN_CLKOUT_CKSEL_XT1KHZ,                // XTAL / 32          = 1.024 KHz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV256    = CLKGEN_CLKOUT_CKSEL_XT128HZ,               // XTAL / 256         = 128 Hz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV8K     = CLKGEN_CLKOUT_CKSEL_XT4HZ,                 // XTAL / 8192        = 4 Hz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV64K    = CLKGEN_CLKOUT_CKSEL_XT12HZ,                // XTAL / 65536       = 0.5 Hz
    AM_HAL_CLKGEN_CLKOUT_XTAL_DIV2M     = CLKGEN_CLKOUT_CKSEL_XT164HZ,               // XTAL / 2M          = 0.015625 Hz

    // HFRC
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV8      = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV4,          // HFRC / 8           ~ 62.5 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV16     = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV8,          // HFRC / 16          ~ 31.25 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV32     = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV16,         // HFRC / 32          ~ 15.625 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV64     = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV32,         // HFRC / 64          ~ 7.8125 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV128    = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV64,         // HFRC / 128         ~ 3.9063 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV256    = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV128,        // HFRC / 256         ~ 1.9531 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV512    = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV256,        // HFRC / 512         ~ 976.5625 KHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV64K    = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV512,        // HFRC / 1K          ~ 488.2813 KHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV128K   = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV64K,        // HFRC / 128K        ~ 3.8147 KHz
    AM_HAL_CLKGEN_CLKOUT_HFRC_DIV32M    = CLKGEN_CLKOUT_CKSEL_HFRCDIV2DIV16M,        // HFRC / 32M         ~ 14.9012 Hz

    // HFRC2
    AM_HAL_CLKGEN_CLKOUT_HFRC2_DIV8     = CLKGEN_CLKOUT_CKSEL_HFRC2DIV2DIV4,         // HFRC2 / 8          ~ 49.5 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC2_DIV16    = CLKGEN_CLKOUT_CKSEL_HFRC2DIV2DIV8,         // HFRC2 / 16         ~ 24.75 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC2_DIV32    = CLKGEN_CLKOUT_CKSEL_HFRC2DIV2DIV16,        // HFRC2 / 32         ~ 12.375 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC2_DIV64    = CLKGEN_CLKOUT_CKSEL_HFRC2DIV2DIV32,        // HFRC2 / 64         ~ 6.1875 MHz
    AM_HAL_CLKGEN_CLKOUT_HFRC2_DIV128   = CLKGEN_CLKOUT_CKSEL_HFRC2DIV2DIV64,        // HFRC2 / 128        ~ 3.0938 MHz

    // EXTREFCLK
    AM_HAL_CLKGEN_CLKOUT_EXTREFCLK      = CLKGEN_CLKOUT_CKSEL_EXTREFCLK,             // EXTREFCLK
    AM_HAL_CLKGEN_CLKOUT_EXTREFCLK_DIV2 = CLKGEN_CLKOUT_CKSEL_EXTREFCLKDIV2,         // EXTREFCLK / 2
    AM_HAL_CLKGEN_CLKOUT_EXTREFCLK_DIV4 = CLKGEN_CLKOUT_CKSEL_EXTREFCLKDIV4,         // EXTREFCLK / 4
    AM_HAL_CLKGEN_CLKOUT_EXTREFCLK_DIV8 = CLKGEN_CLKOUT_CKSEL_EXTREFCLKDIV8,         // EXTREFCLK / 8

    // SYSCLK0
    AM_HAL_CLKGEN_CLKOUT_SYSCLK0        = CLKGEN_CLKOUT_CKSEL_SYSCLK0,               // SYSCLK0
    AM_HAL_CLKGEN_CLKOUT_SYSCLK0_DIV2   = CLKGEN_CLKOUT_CKSEL_SYSCLK0DIV2,           // SYSCLK0 / 2
    AM_HAL_CLKGEN_CLKOUT_SYSCLK0_DIV4   = CLKGEN_CLKOUT_CKSEL_SYSCLK0DIV4,           // SYSCLK0 / 4
    AM_HAL_CLKGEN_CLKOUT_SYSCLK0_DIV8   = CLKGEN_CLKOUT_CKSEL_SYSCLK0DIV8,           // SYSCLK0 / 8

    // SYSCLK1
    AM_HAL_CLKGEN_CLKOUT_SYSCLK1        = CLKGEN_CLKOUT_CKSEL_SYSCLK1,               // SYSCLK1
    AM_HAL_CLKGEN_CLKOUT_SYSCLK1_DIV2   = CLKGEN_CLKOUT_CKSEL_SYSCLK1DIV2,           // SYSCLK1 / 2
    AM_HAL_CLKGEN_CLKOUT_SYSCLK1_DIV4   = CLKGEN_CLKOUT_CKSEL_SYSCLK1DIV4,           // SYSCLK1 / 4
    AM_HAL_CLKGEN_CLKOUT_SYSCLK1_DIV8   = CLKGEN_CLKOUT_CKSEL_SYSCLK1DIV8,           // SYSCLK1 / 8

    // SYSCLK3
    AM_HAL_CLKGEN_CLKOUT_SYSCLK3        = CLKGEN_CLKOUT_CKSEL_SYSCLK3,               // SYSCLK3
    AM_HAL_CLKGEN_CLKOUT_SYSCLK3_DIV2   = CLKGEN_CLKOUT_CKSEL_SYSCLK3DIV2,           // SYSCLK3 / 2
    AM_HAL_CLKGEN_CLKOUT_SYSCLK3_DIV4   = CLKGEN_CLKOUT_CKSEL_SYSCLK3DIV4,           // SYSCLK3 / 4
    AM_HAL_CLKGEN_CLKOUT_SYSCLK3_DIV8   = CLKGEN_CLKOUT_CKSEL_SYSCLK3DIV8,           // SYSCLK3 / 8

    // XTAL_HS
    AM_HAL_CLKGEN_CLKOUT_XTAL_HS_DIV2   = CLKGEN_CLKOUT_CKSEL_XTAL24MHZ,             // XTAL_HS / 2        = 24 MHZ
    AM_HAL_CLKGEN_CLKOUT_XTAL_HS_DIV4   = CLKGEN_CLKOUT_CKSEL_XTAL12MHZ,             // XTAL_HS / 4        = 12 MHZ
    AM_HAL_CLKGEN_CLKOUT_XTAL_HS_DIV8   = CLKGEN_CLKOUT_CKSEL_XTAL6MHZ,              // XTAL_HS / 8        = 6 MHZ

    //AO CLK
    AM_HAL_CLKGEN_CLKOUT_AO_CLK_DIV16   = CLKGEN_CLKOUT_CKSEL_AODIVCLK,               // AO_CLK div16

    // Secure RTC clocks
    AM_HAL_CLKGEN_CLKOUT_RTC_100HZ      = CLKGEN_CLKOUT_CKSEL_CG100HZ,                // Secure RTC 100Hz
    AM_HAL_CLKGEN_CLKOUT_RTC_1HZ        = CLKGEN_CLKOUT_CKSEL_RTC1HZ100,              // Secure RTC 1Hz

} am_hal_clkgen_clkout_e;

#define AM_HAL_CLKGEN_CLKOUT_MAX        AM_HAL_CLKGEN_CLKOUT_AO_CLK_DIV16            // Highest valid CKSEL enum value

// #### INTERNAL BEGIN ####
//
// XTAL 24MHz control
//
/*typedef enum
{
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_0 = 0x0,      // Drive strength
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_1,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_2,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_3,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_4,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_5,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_6,
    AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_7,
} am_hal_clkgen_xtal24mctrl_e;*/

//
// Display clock control
//
/*typedef enum
{
    AM_HAL_CLKGEN_DISPCLKCTRL_DCCLKEN,
    AM_HAL_CLKGEN_DISPCLKCTRL_DISPCLKSEL_OFF,
    AM_HAL_CLKGEN_DISPCLKCTRL_DISPCLKSEL_HFRC48,
    AM_HAL_CLKGEN_DISPCLKCTRL_DISPCLKSEL_HFRC96,
    AM_HAL_CLKGEN_DISPCLKCTRL_DISPCLKSEL_DPHYPLL,
    AM_HAL_CLKGEN_DISPCLKCTRL_PLLCLKEN,
    AM_HAL_CLKGEN_DISPCLKCTRL_PLLCLKSEL_OFF,
    AM_HAL_CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFRC12,
    AM_HAL_CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFRC24,
    AM_HAL_CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFXT,
} am_hal_clkgen_dispclkctrl_e*/;

//
// ClkGen Interrupts
//
/*typedef enum
{
    AM_HAL_CLKGEN_INTERRUPT_OF          = CLKGEN_INTRPTEN_OF_Msk,
    AM_HAL_CLKGEN_INTERRUPT_ACC         = CLKGEN_INTRPTEN_ACC_Msk,
    AM_HAL_CLKGEN_INTERRUPT_ACF         = CLKGEN_INTRPTEN_ACF_Msk
} am_hal_clkgen_interrupt_e;*/
// #### INTERNAL END ####

// //! HFRC ADJ enum for adjust enable
typedef enum
{
    AM_HAL_CLKGEN_HFRCAJD_EN_DIS = CLKGEN_HFADJ_HFADJEN_DIS,
    AM_HAL_CLKGEN_HFRCAJD_EN_EN = CLKGEN_HFADJ_HFADJEN_EN,
} am_hal_clkgen_hfrcadj_enable_e;
//! HFRC ADJ enum for adjust enable

//! HFRC ADJ enum for repeat period
typedef enum
{
    AM_HAL_CLKGEN_HFRCADJ_RPT_4_SEC    = CLKGEN_HFADJ_HFADJCK_4SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_16_SEC   = CLKGEN_HFADJ_HFADJCK_16SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_32_SEC   = CLKGEN_HFADJ_HFADJCK_32SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_64_SEC   = CLKGEN_HFADJ_HFADJCK_64SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_128_SEC  = CLKGEN_HFADJ_HFADJCK_128SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_256_SEC  = CLKGEN_HFADJ_HFADJCK_256SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_512_SEC  = CLKGEN_HFADJ_HFADJCK_512SEC,
    AM_HAL_CLKGEN_HFRCADJ_RPT_1024_SEC = CLKGEN_HFADJ_HFADJCK_1024SEC,
} am_hal_clkgen_hfrcadj_repeat_period_e;

//! HFRC ADJ enum for XT warmup time
typedef enum
{
    AM_HAL_CLKGEN_HFRCAJD_XTWARMUP_1SEC = CLKGEN_HFADJ_HFWARMUP_1SEC,
    AM_HAL_CLKGEN_HFRCAJD_XTWARMUP_2SEC = CLKGEN_HFADJ_HFWARMUP_2SEC,
} am_hal_clkgen_hfrcadj_warmup_e;

//! HFRC ADJ enum for attack speed
typedef enum
{
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1       = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1,       // Fastest Attack Speed
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_2  = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1_in_2,
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_4  = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1_in_4,
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_8  = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1_in_8,
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_16 = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1_in_16,
    AM_HAL_CLKGEN_HFRCADJ_ATTACK_SPEED_1_IN_32 = CLKGEN_HFADJ_HFADJGAIN_Gain_of_1_in_32, // Slowest Attack Speed
} am_hal_clkgen_hfrcadj_attack_speed_e;

//! HFRC ADJ enum for maxdelta enable
typedef enum
{
    AM_HAL_CLKGEN_HFRCADJ_MAXDELTA_DIS = CLKGEN_HFADJ_HFADJMAXDELTA_DISABLED,
    AM_HAL_CLKGEN_HFRCADJ_MAXDELTA_EN  = CLKGEN_HFADJ_HFADJMAXDELTA_ENABLED,
} am_hal_clkgen_hfrcadj_maxdelta_en_e;

//
//! Status structure.
//
typedef struct
{
    //
    // ui32SysclkFreq
    //!  Returns the current system clock frequency, in hertz.
    // TODO: define a clearer term than "system clock frequency"
    uint32_t    ui32SysclkFreq;

    //
    // eRTCOSC
    //
    //!  Returns the current RTC oscillator as one of:
    //!  AM_HAL_CLKGEN_STATUS_RTCOSC_LFRC
    //!  AM_HAL_CLKGEN_STATUS_RTCOSC_XTAL
    //
    uint32_t    eRTCOSC;

    //
    // bXtalFailure
    //!  true = XTAL has failed (is enabled but not oscillating).  Also if the
    //!         LFRC is selected as the oscillator in OCTRL.RTCOSEL.
    //
    bool        bXtalFailure;

    //
    // enable status for all the peripheral clocks.
    //  1: enable
    //  0: disable
    //
    //uint32_t    ui32Clockenstat;
    //uint32_t    ui32Clocken2stat;
    //uint32_t    ui32Clocken3stat;
} am_hal_clkgen_status_t;

//
//! HFRC ADJ Structure
//
typedef union
{
    uint32_t HFRCAdj;

    struct
    {
        am_hal_clkgen_hfrcadj_enable_e        eEnable       : 1;
        am_hal_clkgen_hfrcadj_repeat_period_e eRepeat       : 3;
        uint32_t                                            : 4;
        uint32_t                              ui32TargetVal : 12;
        am_hal_clkgen_hfrcadj_warmup_e        eWarmup       : 1;
        am_hal_clkgen_hfrcadj_attack_speed_e  eSpeed        : 3;
        am_hal_clkgen_hfrcadj_maxdelta_en_e   eMaxDelta     : 5;
        uint32_t                                            : 3;
    } HFRCAdj_b;
} am_hal_clkgen_hfrcadj_t;

// #### INTERNAL BEGIN ####
#if 0   // FAL-778 RevB0 removes OCTRL.STOPRC, OCTRL.STOPXT, and STATUS (RTCOSC, OSCF)
//
// Clkgen status and returns.
//
typedef enum
{
    AM_HAL_CLKGEN_STATUS_SYSCLK,
    AM_HAL_CLKGEN_STATUS_OSC_RTC,
    AM_HAL_CLKGEN_STATUS_XTAL_FAILURE
} am_hal_clkgen_status_e;
#endif
// #### INTERNAL END ####
// ****************************************************************************
//
//! @brief Apply various specific commands/controls on the CLKGEN module.
//!
//! This function is used to apply various controls on CLKGEN.
//!
//! @note IMPORTANT! This function MUST be called very early in execution of
//!       an application with the parameter AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX
//!       in order to set Atomiq11x to its required operating frequency.
//!
//! @param eControl - One of the following:
//!     AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX
//!     AM_HAL_CLKGEN_CONTROL_RTC_SEL_XTAL
//!     AM_HAL_CLKGEN_CONTROL_RTC_SEL_LFRC
//!     AM_HAL_CLKGEN_CONTROL_HFADJ_ENABLE
//!     AM_HAL_CLKGEN_CONTROL_HFADJ_DISABLE
//! @param pArgs - Pointer to arguments for Control Switch Case
//!
//! @return status      - generic or interface specific status.
//!
//! @note After starting the XTAL, a 2 second warm-up delay is required.
//
// ****************************************************************************
extern uint32_t am_hal_clkgen_control(am_hal_clkgen_control_e eControl,
                                      void *pArgs);

// ****************************************************************************
//
//! @brief Get CLKGEN status.
//!
//! This function returns the current value of various CLKGEN statuses.
//!
//! @param psStatus - ptr to a status structure to receive the current statuses.
//!
//! @return status      - generic or interface specific status.
//!
//! @note After selection of the RTC Oscillator, a 2 second delay is required
//! before the new oscillator takes effect.  Therefore the CLKGEN.STATUS.OMODE
//! bit will not reflect the new status until after the 2s wait period.
//
// ****************************************************************************
extern uint32_t am_hal_clkgen_status_get(am_hal_clkgen_status_t *psStatus);

// ****************************************************************************
//
//! @brief Enable CLKOUT.
//!
//! This function is used to enable and select a CLKOUT frequency.
//!
//! @param bEnable: true to enable, false to disable.
//! @param eClkSelect - One of the following:
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_32768
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_16384
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_8192
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_4096
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_2048
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_1024
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_128
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_4
//!     AM_HAL_CLKGEN_CLKOUT_XTAL_0_5
//!
//!     AM_HAL_CLKGEN_CLKOUT_RTC_1Hz
//!     AM_HAL_CLKGEN_CLKOUT_RTC_100Hz
//!
//!     AM_HAL_CLKGEN_CLKOUT_LFRC
//!     AM_HAL_CLKGEN_CLKOUT_LFRC_512
//!     AM_HAL_CLKGEN_CLKOUT_LFRC_32
//!     AM_HAL_CLKGEN_CLKOUT_LFRC_2
//!
//!     AM_HAL_CLKGEN_CLKOUT_PLL_POSTDIV
//!     AM_HAL_CLKGEN_CLKOUT_PLL_OUT2
//!     AM_HAL_CLKGEN_CLKOUT_PLL_OUT3
//!     AM_HAL_CLKGEN_CLKOUT_PLL_OUT4
//!
//!     AM_HAL_CLKGEN_CLKOUT_RF_XTAL_48M
//!     AM_HAL_CLKGEN_CLKOUT_RF_XTAL_24M
//!     AM_HAL_CLKGEN_CLKOUT_RF_XTAL_12M
//!
//!     AM_HAL_CLKGEN_CLKOUT_SECURE_RTC_1HZ
//!     AM_HAL_CLKGEN_CLKOUT_SECURE_RTC_100HZ
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
extern uint32_t am_hal_clkgen_clkout_enable(bool bEnable,
                                            am_hal_clkgen_clkout_e eClkSelect);
// #### INTERNAL BEGIN ####
// ****************************************************************************
//
//! @brief Enable XTAL 24MHz.
//!
//! This function is used to enable XTAL 24MHz and select drive strength.
//!
//! @param bEnable: true to enable, false to disable.
//! @param eDSSelect - One of the following:
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_0
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_1
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_2
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_3
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_4
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_5
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_6
//!     AM_HAL_CLKGEN_XTAL24MCTRL_XTAL24MDS_7
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_xtal24mctrl_enable(bool bEnable,
                                               //am_hal_clkgen_xtal24mctrl_e eDSSelect);

// ****************************************************************************
//
//! @brief Enable DCCLK.
//!
//! This function is used to enable DCCLK and select clock source.
//!
//! @param bEnable: true to enable, false to disable.
//! @param eDSSelect - One of the following:
//!     CLKGEN_DISPCLKCTRL_DISPCLKSEL_OFF
//!     CLKGEN_DISPCLKCTRL_DISPCLKSEL_HFRC48
//!     CLKGEN_DISPCLKCTRL_DISPCLKSEL_HFRC96
//!     CLKGEN_DISPCLKCTRL_DISPCLKSEL_DPHYPLL
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_dcclk_enable(bool bEnable,
                                           //CLKGEN_DISPCLKCTRL_DISPCLKSEL_Enum eDispclkSelect);

// ****************************************************************************
//
//! @brief Enable PLLCLK.
//!
//! This function is used to enable PLLCLK and select pll reference clock.
//!
//! @param bEnable: true to enable, false to disable.
//! @param eDSSelect - One of the following:
//!     CLKGEN_DISPCLKCTRL_PLLCLKSEL_OFF
//!     CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFRC12
//!     CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFRC24
//!     CLKGEN_DISPCLKCTRL_PLLCLKSEL_HFXT
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_pllclk_enable(bool bEnable,
                                          //CLKGEN_DISPCLKCTRL_PLLCLKSEL_Enum ePllclkSelect);

// ****************************************************************************
//
//! @brief Enable selected CLKGEN Interrupts.
//!
//! Use this function to enable the interrupts.
//!
//! @param ui32IntMask - One or more of the following bitmasks.
//!     AM_HAL_CLKGEN_INTERRUPT_OF
//!     AM_HAL_CLKGEN_INTERRUPT_ACC
//!     AM_HAL_CLKGEN_INTERRUPT_ACF
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_interrupt_enable(am_hal_clkgen_interrupt_e ui32IntMask);

// ****************************************************************************
//
//! @brief Disable selected CLKGEN Interrupts.
//!
//! Use this function to disable the CLKGEN interrupts.
//!
//! @param ui32IntMask - One or more of the following bitmasks.
//!     AM_HAL_CLKGEN_INTERRUPT_OF
//!     AM_HAL_CLKGEN_INTERRUPT_ACC
//!     AM_HAL_CLKGEN_INTERRUPT_ACF
//!
//! @return status      - generic or interface specific status.
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_interrupt_disable(am_hal_clkgen_interrupt_e ui32IntMask);

//*****************************************************************************
//
//! @brief IOM interrupt clear
//!
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function clears the interrupts for the given peripheral.
//!
//! The following are valid clear bits, any of which can be ORed together.
//!     AM_HAL_CLKGEN_INTERRUPT_OF
//!     AM_HAL_CLKGEN_INTERRUPT_ACC
//!     AM_HAL_CLKGEN_INTERRUPT_ACF
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
//extern uint32_t am_hal_clkgen_interrupt_clear(am_hal_clkgen_interrupt_e ui32IntMask);

// ****************************************************************************
//
//! @brief Return CLKGEN interrupts.
//!
//! Use this function to get all CLKGEN interrupts, or only the interrupts
//! that are enabled.
//!
//! @return status
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_interrupt_status_get(bool bEnabledOnly,
//                                                   uint32_t *pui32IntStatus);

// ****************************************************************************
//
//! @brief Sets the interrupt status.
//!
//! This function sets the CLKGEN interrupts.
//!
//! @param ui32IntMask - One or more of the following bitmasks.
//!     AM_HAL_CLKGEN_INTERRUPT_OF
//!     AM_HAL_CLKGEN_INTERRUPT_ACC
//!     AM_HAL_CLKGEN_INTERRUPT_ACF
//!
//! @return status
//
// ****************************************************************************
//extern uint32_t am_hal_clkgen_interrupt_set(am_hal_clkgen_interrupt_e ui32IntMask);
// #### INTERNAL END ####
#ifdef __cplusplus
}
#endif

#endif // AM_HAL_CLKGEN_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
