//*****************************************************************************
//
//! @file am_sdk_version.h
//!
//! @brief Defines SDK version.
//!
//! @addtogroup ambiqsuite Ambiqsuite SDK
//
//! @defgroup hal mcu
//! @ingroup ambiqsuite
//! @{

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_SDK_VERSION_H
#define AM_SDK_VERSION_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Macros to define HAL SDK version.
//
//*****************************************************************************
//
// Define the current HAL version.
//
#ifndef AM_HAL_VERSION_MAJ
#if defined(AM_PART_APOLLO3_API)
#define AM_HAL_VERSION_MAJ      3
#define AM_HAL_VERSION_MIN      2
#define AM_HAL_VERSION_REV      0
#elif defined(AM_PART_APOLLO4_API)
#define AM_HAL_VERSION_MAJ      4
#define AM_HAL_VERSION_MIN      5
#define AM_HAL_VERSION_REV      0
#elif defined(AM_PART_APOLLO5_API)
#define AM_HAL_VERSION_MAJ      5
#define AM_HAL_VERSION_MIN      2
#define AM_HAL_VERSION_REV      0
#elif defined(AM_PART_ATOMIQ11X_API)
#define AM_HAL_VERSION_MAJ      6
#define AM_HAL_VERSION_MIN      1
#define AM_HAL_VERSION_REV      0
#else
#define AM_HAL_VERSION_MAJ      0
#define AM_HAL_VERSION_MIN      0
#define AM_HAL_VERSION_REV      0
#endif
#endif // AM_HAL_VERSION_MAJ

#if (AM_HAL_VERSION_MAJ == 0)
#error AM_HAL_VERSION_MAJ cannot be defined as 0.
#endif

#ifdef __cplusplus
}
#endif

#endif // AM_SDK_VERSION_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
