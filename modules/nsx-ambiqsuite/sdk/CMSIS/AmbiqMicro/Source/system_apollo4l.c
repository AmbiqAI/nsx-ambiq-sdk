//*****************************************************************************
//
//! @file system_apollo4l.c
//!
//! @brief Ambiq Micro Apollo4 Lite MCU specific functions.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.17 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include "system_apollo4l.h"
#include "apollo4l.h"

//*****************************************************************************
//
// Defines
//
//*****************************************************************************

//
// Clocks
//
#define __HSI             (6000000UL)
#define __XTAL            (32768UL)         // Crystal Oscillator frequency
#define __SYS_OSC_CLK     (96000000)        // Main oscillator frequency
#define __SYSTEM_CLOCK    (1 * __SYS_OSC_CLK)

//
// Initialize SystemCoreClock with the system core clock frequency value
// achieved after system intitialization.
// This means system core clock frequency after call to SystemInit()
//
uint32_t SystemCoreClock = __SYSTEM_CLOCK;  // System Clock Frequency (Core Clock)

//*****************************************************************************
//
//! @brief Set the global clock frequncy.
//!
//! This function sets the global clock frequency.
//!
//! @return None.
//
//*****************************************************************************
void
SystemCoreClockUpdate(void)
{
    //
    // Calculate the system frequency based upon the current register settings.
    // This function can be used to retrieve the system core clock frequeny
    // after user changed register sittings.
    //
    SystemCoreClock = __SYS_OSC_CLK;
}

//*****************************************************************************
//
//! @brief Initialize the system.
//!
//! This function sets up the microcontroller system.
//!
//! @return None.
//
//*****************************************************************************
void
SystemInit(void)
{
    //
    // Initialize the system
    // Do not use global variables because this function is called before
    // reaching pre-main. RW section maybe overwritten afterwards.
    //
    SystemCoreClock = __SYSTEM_CLOCK;
}

