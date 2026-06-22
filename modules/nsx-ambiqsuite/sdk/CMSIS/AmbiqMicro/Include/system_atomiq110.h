//*****************************************************************************
//
//! @file system_atomiq110.h
//!
//! @brief Ambiq Atomiq110 MCU specific functions.
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

#ifndef SYSTEM_ATOMIQ110_H
#define SYSTEM_ATOMIQ110_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
//
// Exception / Interrupt Handler Function Prototype
//
typedef void(*VECTOR_TABLE_Type)(void);

//
// System Clock Frequency (Core Clock)
//
extern uint32_t SystemCoreClock;     // System Clock Frequency (Core Clock)

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************
//
// Initialize the System and update the SystemCoreClock variable.
//
extern void SystemInit (void);
//
// Updates the SystemCoreClock with current core clock retrieved from CPU registers.
//
extern void SystemCoreClockUpdate (void);

#ifdef __cplusplus
}
#endif

#endif  // SYSTEM_ATOMIQ110_H

