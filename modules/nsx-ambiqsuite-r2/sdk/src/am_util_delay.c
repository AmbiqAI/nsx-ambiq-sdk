//*****************************************************************************
//
//! @file am_util_delay.c
//!
//! @brief Delay Utility Functions
//!
//! @addtogroup delay_utils Delay Utility Functions
//! @ingroup utils
//! @{
//!
//! Purpose: This module provides delay utility functions for timing control
//!          across Ambiq Micro devices. It enables accurate software delays,
//!          cycle counting, and timing operations for embedded applications
//!          requiring precise timing control. The utilities support both
//!          microsecond and millisecond delays with calibration options.
//!
//! @section delay_features Key Features
//!
//! 1. @b Cycle @b Delays: Precise cycle-based timing control.
//! 2. @b Millisecond @b Delays: High-level millisecond delay functions.
//! 3. @b Microsecond @b Delays: Precise microsecond timing operations.
//! 4. @b Frequency @b Adaptive: Automatic timing adjustment based on system clock.
//! 5. @b Device @b Specific: Optimized implementations for different Apollo families.
//! 6. @b Power @b Efficient: Low-power delay operations for battery applications.
//!
//! @section delay_functionality Functionality
//!
//! - Provide cycle-based delay functions for precise timing
//! - Support millisecond and microsecond delay operations
//! - Adapt timing based on system clock frequency
//! - Optimize delays for different Apollo device families
//! - Enable power-efficient timing operations
//! - Support hardware-specific delay implementations
//! - Provide consistent timing across device variations
//! - Handle clock frequency variations automatically
//!
//! @section delay_usage Usage
//!
//! 1. Use am_util_delay_cycles() for precise cycle-based delays
//! 2. Call am_util_delay_ms() for millisecond timing
//! 3. Use am_util_delay_us() for microsecond precision
//! 4. System automatically adapts to device clock frequency
//! 5. No manual configuration required for different devices
//!
//! @section delay_configuration Configuration
//!
//! - @b Device @b Support: Apollo3, Apollo4, Apollo5 family compatibility
//! - @b Clock @b Detection: Automatic system clock frequency detection
//! - @b Hardware @b Optimization: Device-specific delay implementations
//! - @b Power @b Management: Low-power delay operations
//! - @b Timing @b Precision: Cycle-accurate timing control
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include  <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"
#include "am_util_delay.h"

//*****************************************************************************
//
// Delays for a desired amount of loops.
//
//*****************************************************************************
void
am_util_delay_cycles(uint32_t ui32Iterations)
{
    //
    // Call the BOOTROM cycle delay function
    //
#if defined(AM_PART_APOLLO) || defined(AM_PART_APOLLO2) || defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    am_hal_flash_delay(ui32Iterations);
#else
    am_hal_delay_us( ui32Iterations);
#endif
}

//*****************************************************************************
//
// Delays for a desired amount of milliseconds.
//
//*****************************************************************************
void
am_util_delay_ms(uint32_t ui32MilliSeconds)
{
#if defined(AM_PART_APOLLO) || defined(AM_PART_APOLLO2) || defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    uint32_t ui32Loops, ui32HFRC;
#if AM_APOLLO3_CLKGEN
    am_hal_clkgen_status_t sClkgenStatus;
    am_hal_clkgen_status_get(&sClkgenStatus);
    ui32HFRC = sClkgenStatus.ui32SysclkFreq;
#else // AM_APOLLO3_CLKGEN
    ui32HFRC = am_hal_clkgen_sysclk_get();
#endif // AM_APOLLO3_CLKGEN
    ui32Loops = ui32MilliSeconds * (ui32HFRC / 3000);

    //
    // Call the BOOTROM cycle delay function
    //
    am_hal_flash_delay(ui32Loops);
#else
    am_hal_delay_us( ui32MilliSeconds * 1000);
#endif
}

//*****************************************************************************
//
// Delays for a desired amount of microseconds.
//
//*****************************************************************************
void
am_util_delay_us(uint32_t ui32MicroSeconds)
{
#if defined(AM_PART_APOLLO) || defined(AM_PART_APOLLO2) || defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P)
    uint32_t ui32Loops, ui32HFRC;

#if AM_APOLLO3_CLKGEN
    am_hal_clkgen_status_t sClkgenStatus;
    am_hal_clkgen_status_get(&sClkgenStatus);
    ui32HFRC = sClkgenStatus.ui32SysclkFreq;
#else // AM_APOLLO3_CLKGEN
    ui32HFRC = am_hal_clkgen_sysclk_get();
#endif // AM_APOLLO3_CLKGEN
    ui32Loops = ui32MicroSeconds * (ui32HFRC / 3000000);

    //
    // Call the BOOTROM cycle delay function
    //
    am_hal_flash_delay(ui32Loops);
#else
    am_hal_delay_us( ui32MicroSeconds );
#endif
}

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

