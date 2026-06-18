//*****************************************************************************
//
//! @file am_devices_led.c
//!
//! @brief Functions for controlling an array of LEDs
//!
//! @addtogroup devices_led LED Devices Generic Driver
//! @ingroup devices
//! @{
//!
//! Purpose: This module provides a hardware abstraction layer
//! for GPIO-based LED output devices with support for multiple drive modes,
//! polarity control, and array-based LED management. It enables efficient
//! LED control, state management, and power-efficient operation for embedded
//! applications requiring visual feedback capabilities. The driver supports
//! multiple LED configurations including direct drive, open drain, and tri-state
//! modes with features like polarity control and array operations
//! for optimal visual interface performance and system integration.
//!
//! @section led_features Key Features
//!
//! 1. @b GPIO @b Control: Direct GPIO-based LED control and management.
//! 2. @b Multiple @b Drive @b Modes: Support for direct drive, open drain, and tri-state.
//! 3. @b Polarity @b Control: Configurable LED polarity and drive direction.
//! 4. @b Array @b Operations: Efficient management of multiple LED arrays.
//! 5. @b State @b Management: Individual LED state control and monitoring.
//! 6. @b Power @b Efficient: Low-power LED control for battery applications.
//!
//! @section led_functionality Functionality
//!
//! - Initialize and configure individual LEDs and LED arrays
//! - Control LED on/off states and toggle operations
//! - Manage multiple drive modes (direct, open drain, tri-state)
//! - Handle polarity control for different LED configurations
//! - Support array-based LED operations and control
//! - Provide LED state monitoring and status checking
//! - Enable power-efficient LED control operations
//! - Support GPIO pin configuration and management
//!
//! @section led_usage Usage
//!
//! 1. Initialize LED using am_devices_led_init() or am_devices_led_array_init()
//! 2. Configure LED polarity and drive mode settings
//! 3. Control LED states with on/off/toggle functions
//! 4. Monitor LED status using get functions
//! 5. Manage array operations for multiple LEDs
//!
//! @section led_configuration Configuration
//!
//! - @b Drive @b Modes: Direct drive, open drain, and tri-state configurations
//! - @b Polarity @b Control: Configurable LED polarity settings
//! - @b GPIO @b Pins: Pin assignment and configuration options
//! - @b Array @b Support: Multiple LED array management
//! - @b Power @b Management: Low-power operation settings
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
#include <stdbool.h>
#include "am_mcu_apollo.h"
#include "am_devices_led.h"

#if                                         \
/* #### INTERNAL BEGIN #### */              \
/* This INTERNAL block is intended to mask AM_PART_ATOMIQ11X_API. But the ATOMIQ11X conditional is also needed for Carrera development. */  \
/* #### INTERNAL END ####   */              \
    defined(AM_PART_ATOMIQ11X_API)      ||  \
    defined(AM_PART_APOLLO5_API)        ||  \
    defined(AM_PART_APOLLO4_API)
#define AM_DEVICES_LED_GPIO_API_AP4     // The GPIO API that began with Apollo4
#endif

//
//! @brief Define a somewhat arbitrary maximum number of LEDs.
//! @details No board is actually expected to have this many LEDs,
//! the value is used for parameter validation.
//
#define MAX_LEDS        31

//*****************************************************************************
//
//  Configures the necessary pins for an array of LEDs
//
//*****************************************************************************
void
am_devices_led_init(am_devices_led_t *psLED)
{
    if ( (psLED == NULL)    ||
         (psLED->ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS) )
    {
        return;
    }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLED->ui32Polarity )
    {
        //
        // Configure the pin as a push-pull GPIO output.
        //
        am_hal_gpio_pinconfig(psLED->ui32GPIONumber, am_hal_gpio_pincfg_output);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.  Note that for Apollo3 GPIOs in push-pull mode, the output
        // enable, normally a tri-state control, instead functions as an enable
        // for Fast GPIO. Its state does not matter on previous chips, so for
        // normal GPIO usage on Apollo3, it must be disabled.
        //
        am_hal_gpio_state_write(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
        am_hal_gpio_state_write(psLED->ui32GPIONumber,
                                psLED->ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else if ( AM_DEVICES_LED_POL_OPEN_DRAIN & psLED->ui32Polarity )
    {
        //
        // Configure the pin as a Open Drain GPIO output.
        //
        am_hal_gpio_pinconfig(psLED->ui32GPIONumber, am_hal_gpio_pincfg_opendrain);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.
        //
        am_hal_gpio_state_write(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
        am_hal_gpio_state_write(psLED->ui32GPIONumber,
                                psLED->ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else
    {
        //
        // Configure the pin as a tri-state GPIO.
        //
        am_hal_gpio_pinconfig(psLED->ui32GPIONumber, am_hal_gpio_pincfg_tristate);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.
        //
        am_hal_gpio_state_write(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
        am_hal_gpio_state_write(psLED->ui32GPIONumber,
                                psLED->ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
#else
#if AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLED->ui32Polarity )
    {
        //
        // Configure the pin as a push-pull GPIO output.
        //
        am_hal_gpio_pinconfig(psLED->ui32GPIONumber, g_AM_HAL_GPIO_OUTPUT);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.  Note that for Apollo3 GPIOs in push-pull mode, the output
        // enable, normally a tri-state control, instead functions as an enable
        // for Fast GPIO. Its state does not matter on previous chips, so for
        // normal GPIO usage on Apollo3, it must be disabled.
        //
        am_hal_gpio_state_write(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
        am_hal_gpio_state_write(psLED->ui32GPIONumber,
                                psLED->ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else
    {
        //
        // Configure the pin as a tri-state GPIO.
        //
        am_hal_gpio_pinconfig(psLED->ui32GPIONumber, g_AM_HAL_GPIO_TRISTATE);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.
        //
        am_hal_gpio_state_write(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
        am_hal_gpio_state_write(psLED->ui32GPIONumber,
                                psLED->ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
#else // AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLED->ui32Polarity )
    {
        //
        // Configure the pin as a push-pull GPIO output.
        //
        am_hal_gpio_pin_config(psLED->ui32GPIONumber, AM_HAL_GPIO_OUTPUT);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.
        //
        am_hal_gpio_out_enable_bit_clear(psLED->ui32GPIONumber);
        am_hal_gpio_out_bit_replace(psLED->ui32GPIONumber,
                                    psLED->ui32Polarity &
                                    AM_DEVICES_LED_POL_POLARITY_M);
    }
    else
    {
        //
        // Configure the pin as a tri-state GPIO.
        //
        am_hal_gpio_pin_config(psLED->ui32GPIONumber, AM_HAL_GPIO_3STATE);

        //
        // Disable the output driver, and set the output value to the LEDs "ON"
        // state.
        //
        am_hal_gpio_out_enable_bit_clear(psLED->ui32GPIONumber);
        am_hal_gpio_out_bit_replace(psLED->ui32GPIONumber,
                                    psLED->ui32Polarity &
                                    AM_DEVICES_LED_POL_POLARITY_M );
    }
#endif // AM_APOLLO3_GPIO
#endif
}

//*****************************************************************************
//
//  Disables an array of LEDs
//
//*****************************************************************************
void
am_devices_led_array_disable(am_devices_led_t *psLEDs, uint32_t ui32NumLEDs)
{
    if ( (psLEDs == NULL)                       ||
         (ui32NumLEDs > MAX_LEDS) )
    {
        return;
    }

    //
    // Loop through the list of LEDs, configuring each one individually.
    //
    for ( uint32_t i = 0; i < ui32NumLEDs; i++ )
    {
        if ( psLEDs[i].ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS )
        {
            continue;
        }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
        am_hal_gpio_pinconfig((psLEDs + i)->ui32GPIONumber, am_hal_gpio_pincfg_disabled);
#else
#if AM_APOLLO3_GPIO
        am_hal_gpio_pinconfig((psLEDs + i)->ui32GPIONumber, g_AM_HAL_GPIO_DISABLE);
#else // AM_APOLLO3_GPIO
        am_hal_gpio_pin_config((psLEDs + i)->ui32GPIONumber, AM_HAL_GPIO_DISABLE);
#endif // AM_APOLLO3_GPIO
#endif
    }
}

//*****************************************************************************
//
//  Configures the necessary pins for an array of LEDs
//
//*****************************************************************************
void
am_devices_led_array_init(am_devices_led_t *psLEDs, uint32_t ui32NumLEDs)
{
    uint32_t i;

    if ( (psLEDs == NULL)                       ||
         (ui32NumLEDs > MAX_LEDS) )
    {
        return;
    }

    //
    // Loop through the list of LEDs, configuring each one individually.
    //
    for ( i = 0; i < ui32NumLEDs; i++ )
    {
        am_devices_led_init(psLEDs + i);
    }
}

//*****************************************************************************
//
//  Turns on the requested LED.
//
//*****************************************************************************
void
am_devices_led_on(am_devices_led_t *psLEDs, uint32_t ui32LEDNum)
{
    if ( (psLEDs == NULL)                       ||
         (ui32LEDNum >= MAX_LEDS)               ||
         (psLEDs[ui32LEDNum].ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS) )
    {
        return;
    }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else if ( AM_DEVICES_LED_POL_OPEN_DRAIN & psLEDs->ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else
    {
        //
        // Turn on the output driver for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TRISTATE_ENABLE);
    }
#else
#if (1 == AM_APOLLO3_GPIO)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
    }
    else
    {
        //
        // Turn on the output driver for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TRISTATE_ENABLE);
    }
#else // AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_out_bit_replace(psLEDs[ui32LEDNum].ui32GPIONumber,
                                    psLEDs[ui32LEDNum].ui32Polarity &
                                    AM_DEVICES_LED_POL_POLARITY_M );
    }
    else
    {
        //
        // Turn on the output driver for the LED.
        //
        am_hal_gpio_out_enable_bit_set(psLEDs[ui32LEDNum].ui32GPIONumber);
    }
#endif // AM_APOLLO3_GPIO
#endif
}

//*****************************************************************************
//
//  Turns off the requested LED.
//
//*****************************************************************************
void
am_devices_led_off(am_devices_led_t *psLEDs, uint32_t ui32LEDNum)
{
    if ( (psLEDs == NULL)                       ||
         (ui32LEDNum >= MAX_LEDS)               ||
         (psLEDs[ui32LEDNum].ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS) )
    {
        return;
    }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);
    }
    else if ( AM_DEVICES_LED_POL_OPEN_DRAIN & psLEDs->ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);
    }
    else
    {
        //
        // Turn off the output driver for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
    }
#else
#if (1 == AM_APOLLO3_GPIO)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                psLEDs[ui32LEDNum].ui32Polarity & AM_DEVICES_LED_POL_POLARITY_M ?
                                AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);
    }
    else
    {
        //
        // Turn off the output driver for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
    }
#else // AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_out_bit_replace(psLEDs[ui32LEDNum].ui32GPIONumber,
                                    !(psLEDs[ui32LEDNum].ui32Polarity &
                                      AM_DEVICES_LED_POL_POLARITY_M) );
    }
    else
    {
        //
        // Turn off the output driver for the LED.
        //
        am_hal_gpio_out_enable_bit_clear(psLEDs[ui32LEDNum].ui32GPIONumber);
    }
#endif // AM_APOLLO3_GPIO
#endif
}

//*****************************************************************************
//
//  Toggles the requested LED.
//
//*****************************************************************************
void
am_devices_led_toggle(am_devices_led_t *psLEDs, uint32_t ui32LEDNum)
{
    if ( (psLEDs == NULL)                       ||
         (ui32LEDNum >= MAX_LEDS)               ||
         (psLEDs[ui32LEDNum].ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS) )
    {
        return;
    }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TOGGLE);
    }
    else if ( AM_DEVICES_LED_POL_OPEN_DRAIN & psLEDs->ui32Polarity )
    {
        //
        // Set the output to the correct state for the LED.
        //
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TOGGLE);
    }
    else
    {
        uint32_t ui32Ret, ui32Value;

        //
        // Check to see if the LED pin is enabled.
        //
        ui32Ret = am_hal_gpio_state_read(psLEDs[ui32LEDNum].ui32GPIONumber,
                                         AM_HAL_GPIO_ENABLE_READ, &ui32Value);

        if ( ui32Ret == AM_HAL_STATUS_SUCCESS )
        {
            if ( ui32Value )
            {
                //
                // If it was enabled, turn if off.
                //
                am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                        AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
            }
            else
            {
                //
                // If it was not enabled, turn it on.
                //
                am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                        AM_HAL_GPIO_OUTPUT_TRISTATE_ENABLE);
            }
        }
    }
#else
#if (1 == AM_APOLLO3_GPIO)
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                AM_HAL_GPIO_OUTPUT_TOGGLE);
    }
    else
    {
        uint32_t ui32Ret, ui32Value;

        //
        // Check to see if the LED pin is enabled.
        //
        ui32Ret = am_hal_gpio_state_read(psLEDs[ui32LEDNum].ui32GPIONumber,
                                         AM_HAL_GPIO_ENABLE_READ, &ui32Value);

        if ( ui32Ret == AM_HAL_STATUS_SUCCESS )
        {
            if ( ui32Value )
            {
                //
                // If it was enabled, turn if off.
                //
                am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                        AM_HAL_GPIO_OUTPUT_TRISTATE_DISABLE);
            }
            else
            {
                //
                // If it was not enabled, turn it on.
                //
                am_hal_gpio_state_write(psLEDs[ui32LEDNum].ui32GPIONumber,
                                        AM_HAL_GPIO_OUTPUT_TRISTATE_ENABLE);
            }
        }
    }
#else // AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        am_hal_gpio_out_bit_toggle(psLEDs[ui32LEDNum].ui32GPIONumber);
    }
    else
    {
        //
        // Check to see if the LED pin is enabled.
        //
        if ( am_hal_gpio_out_enable_bit_get(psLEDs[ui32LEDNum].ui32GPIONumber) )
        {
            //
            // If it was enabled, turn if off.
            //
            am_hal_gpio_out_enable_bit_clear(psLEDs[ui32LEDNum].ui32GPIONumber);
        }
        else
        {
            //
            // If it was not enabled, turn if on.
            //
            am_hal_gpio_out_enable_bit_set(psLEDs[ui32LEDNum].ui32GPIONumber);
        }
    }
#endif // AM_APOLLO3_GPIO
#endif
}

//*****************************************************************************
//
//  Gets the state of the requested LED.
//
//*****************************************************************************
bool
am_devices_led_get(am_devices_led_t *psLEDs, uint32_t ui32LEDNum)
{
    if ( (psLEDs == NULL)                       ||
         (ui32LEDNum >= MAX_LEDS)               ||
         (psLEDs[ui32LEDNum].ui32GPIONumber >= AM_HAL_GPIO_MAX_PADS) )
    {
        return false;   // No error return, so return as off
    }

#if defined(AM_DEVICES_LED_GPIO_API_AP4)
    uint32_t ui32Ret, ui32Value;
    am_hal_gpio_read_type_e eReadType;

    eReadType = AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity ?
                AM_HAL_GPIO_OUTPUT_READ : AM_HAL_GPIO_ENABLE_READ;

    ui32Ret = am_hal_gpio_state_read(psLEDs[ui32LEDNum].ui32GPIONumber,
                                     eReadType, &ui32Value);

    if ( ui32Ret == AM_HAL_STATUS_SUCCESS )
    {
        return (bool)ui32Value;
    }
    else
    {
        return false;
    }
#else
#if (1 == AM_APOLLO3_GPIO)
    uint32_t ui32Ret, ui32Value;
    am_hal_gpio_read_type_e eReadType;

    eReadType = AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity ?
                AM_HAL_GPIO_OUTPUT_READ : AM_HAL_GPIO_ENABLE_READ;

    ui32Ret = am_hal_gpio_state_read(psLEDs[ui32LEDNum].ui32GPIONumber,
                                     eReadType, &ui32Value);

    if ( ui32Ret == AM_HAL_STATUS_SUCCESS )
    {
        return (bool)ui32Value;
    }
    else
    {
        return false;
    }
#else // AM_APOLLO3_GPIO
    //
    // Handle Direct Drive Versus 3-State (with pull-up or no buffer).
    //
    if ( AM_DEVICES_LED_POL_DIRECT_DRIVE_M & psLEDs[ui32LEDNum].ui32Polarity )
    {
        //
        // Mask to the GPIO bit position for this GPIO number.
        //
        uint64_t ui64Mask = ((uint64_t)0x01l) << psLEDs[ui32LEDNum].ui32GPIONumber;

        //
        // Extract the state of this bit and return it.
        //
        return !!(am_hal_gpio_out_read() & ui64Mask);
    }
    else
    {
        return am_hal_gpio_out_enable_bit_get(psLEDs[ui32LEDNum].ui32GPIONumber);
    }
#endif // AM_APOLLO3_GPIO
#endif
}

//*****************************************************************************
//
//  Display a binary value using LEDs.
//
//*****************************************************************************
void
am_devices_led_array_out(am_devices_led_t *psLEDs, uint32_t ui32NumLEDs,
                         uint32_t ui32Value)
{
    uint32_t i;

    for ( i = 0; i < ui32NumLEDs; i++ )
    {
        if ( ui32Value & (1 << i) )
        {
            am_devices_led_on(psLEDs, i);
        }
        else
        {
            am_devices_led_off(psLEDs, i);
        }
    }
}
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

