//*****************************************************************************
//
//! @file am_devices_display_ls014b7dd01.c
//!
//! @brief Driver for the SHARP LS014B7DD01 memory LCD over the JDI interface.
//!
//! Configures NemaDC JDI timing and generates VCOM/VA PWM signals via a
//! general-purpose timer. The panel destination color format is fixed to RGB222.
//!
//! @addtogroup devices_display_ls014b7dd01 SHARP DC JDI Driver
//! @ingroup devices
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include "am_devices_display_ls014b7dd01.h"
#include "am_bsp.h"

//
// VCOM duty cycle (percent). VA is driven in phase; VCOM is driven inverted.
//
#define SHARP_VA_VCOM_DUTYCYCLE     50

typedef struct
{
    uint32_t                            ui32Module;
    void                                *pDCHandle;
    bool                                bOccupied;
    am_devices_display_timer_config_t   *pTimerCfg;
} am_devices_ls014b7dd01_t;

am_devices_ls014b7dd01_t gLs014bdd01[AM_REG_DC_NUM_MODULES];

//*****************************************************************************
//
// Fill in NemaDC configuration for the LS014B7DD01 JDI panel.
//
//*****************************************************************************
uint32_t
am_devices_display_ls014b7dd01_get_parameter(void *pHandle,
                                             am_hal_dc_config_t *pDCCfg)
{
    pDCCfg->eInterface = AM_HAL_DC_IF_JDI;
    pDCCfg->bTEEnable = false;
    //
    // The destination color format is fixed as RGB222.
    //

    //
    // JDI timing values are derived from the panel datasheet and
    // MiP_PanelsAppNote.pdf. Resolution is 280 x 280 pixels.
    //
    pDCCfg->sJDICfg.sJDITiming.resx = 280;
    pDCCfg->sJDICfg.sJDITiming.resy = 280;
    pDCCfg->sJDICfg.sJDITiming.XRST_INTB_delay = 1;
    pDCCfg->sJDICfg.sJDITiming.XRST_INTB_width = 566;
    pDCCfg->sJDICfg.sJDITiming.VST_GSP_delay = 73;
    pDCCfg->sJDICfg.sJDITiming.VST_GSP_width = 576;
    pDCCfg->sJDICfg.sJDITiming.VCK_GCK_delay = 217;
    pDCCfg->sJDICfg.sJDITiming.VCK_GCK_width = 288;
    pDCCfg->sJDICfg.sJDITiming.VCK_GCK_closing_pulses = 6;
    pDCCfg->sJDICfg.sJDITiming.HST_BSP_delay = 2;
    pDCCfg->sJDICfg.sJDITiming.HST_BSP_width = 4;
    pDCCfg->sJDICfg.sJDITiming.HCK_BCK_data_start = 1;
    pDCCfg->sJDICfg.sJDITiming.ENB_GEN_delay = 90;
    pDCCfg->sJDICfg.sJDITiming.ENB_GEN_width = 99;

    pDCCfg->sJDICfg.fTargetHCKBCKClk = 0.5;   // MHz
    pDCCfg->sJDICfg.fTargetVCKGCKFF = 0.758;  // MHz

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Configure the timer that drives JDI VCOM/VA PWM signals.
//
//*****************************************************************************
uint32_t
am_devices_display_ls014b7dd01_init(uint32_t ui32Module,
                                    am_devices_display_timer_config_t *pDevCfg,
                                    void **ppHandle, void *pDCHandle)
{
    uint64_t ui64BaseFreq, ui64EndCounts;
    am_hal_timer_config_t TimerConfig ;

    if(ui32Module > AM_REG_DC_NUM_MODULES)
    {
        return AM_HAL_STATUS_OUT_OF_RANGE;
    }

    am_hal_timer_default_config_set( &TimerConfig ) ;
    TimerConfig.eFunction = AM_HAL_TIMER_FN_PWM;
    TimerConfig.eInputClock = pDevCfg->eTimerClock;
    //
    // Configure the TIMER.
    //
    am_hal_timer_config(pDevCfg->ui32TimerNum, &TimerConfig);
    //
    // Route timer outputs to BSP VA and VCOM pins. OUT1 drives VA in phase;
    // OUT0 drives VCOM with the complementary duty cycle.
    //
    am_hal_timer_output_config(am_bsp_disp_jdi_timer_pins(0),
                               AM_HAL_TIMER_OUTPUT_TMR0_OUT1 + pDevCfg->ui32TimerNum * 2);
    am_hal_timer_output_config(am_bsp_disp_jdi_timer_pins(1),
                               AM_HAL_TIMER_OUTPUT_TMR0_OUT0 + pDevCfg->ui32TimerNum * 2);
// #### INTERNAL BEGIN ####
#ifdef ATOMIQ11X_FPGA
    ui64BaseFreq = (ATOMIQ11X_FPGA * 1000000ull) >> (2 * pDevCfg->eTimerClock + 2);
#else
// #### INTERNAL END ####

    ui64BaseFreq = 96000000ull >> (2 * pDevCfg->eTimerClock + 2);

// #### INTERNAL BEGIN ####
#endif
// #### INTERNAL END ####
    //
    // Program compare values for the requested PWM frequency and VCOM duty.
    //
    ui64EndCounts = (uint64_t)(ui64BaseFreq / pDevCfg->ui32Frequency + 0.5f);
    TimerConfig.ui32Compare0    = (uint32_t) ui64EndCounts;
    am_hal_timer_compare0_set(pDevCfg->ui32TimerNum, TimerConfig.ui32Compare0);
    TimerConfig.ui32Compare1 = (uint32_t)(ui64EndCounts * (1 - SHARP_VA_VCOM_DUTYCYCLE / 100.0f) + 0.5f);
    am_hal_timer_compare1_set(pDevCfg->ui32TimerNum, TimerConfig.ui32Compare1);

    gLs014bdd01[ui32Module].ui32Module = ui32Module;
    gLs014bdd01[ui32Module].bOccupied = true;
    gLs014bdd01[ui32Module].pDCHandle = pDCHandle;
    gLs014bdd01[ui32Module].pTimerCfg = pDevCfg;

    *ppHandle = (void *)&gLs014bdd01[ui32Module];

    return AM_DEVICES_DISPLAY_STATUS_SUCCESS;
}

//*****************************************************************************
//
// Enable the JDI VCOM/VA PWM timer.
//
//*****************************************************************************
uint32_t
am_devices_display_ls014b7dd01_timer_enable(void *pHandle)
{
    am_devices_ls014b7dd01_t *pDevice = (am_devices_ls014b7dd01_t *)pHandle;
    //
    // Enable the TIMER.
    //
    return am_hal_timer_enable(pDevice->pTimerCfg->ui32TimerNum);
}

//*****************************************************************************
//
// Disable the JDI VCOM/VA PWM timer.
//
//*****************************************************************************
uint32_t
am_devices_display_ls014b7dd01_timer_disable(void *pHandle)
{
    am_devices_ls014b7dd01_t *pDevice = (am_devices_ls014b7dd01_t *)pHandle;
    //
    // Disable the TIMER.
    //
    return am_hal_timer_disable(pDevice->pTimerCfg->ui32TimerNum);
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
