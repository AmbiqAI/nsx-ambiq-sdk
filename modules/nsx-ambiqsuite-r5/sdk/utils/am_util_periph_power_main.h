//*****************************************************************************
//
//! @file am_util_periph_power_main.h
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_UTIL_PERIPH_POWER_MAIN_H
#define AM_UTIL_PERIPH_POWER_MAIN_H

// Define the magic number values for invalid data and exit program.
#define PERIPH_POWER_RUN_INVALID     0xCC80F1C8
#define PERIPH_POWER_RUN_EXIT        0x4653CF23

typedef struct
{
    uint32_t          ui32TriggerGPIO;        // GPIO to be used for power test measurement trigger.
    uint32_t          ui32RunStatus;       // Status (valid/invalid or exit).

    uint32_t          ui32Index;

    const void        *pCapabilities;          // Array of pointers to const capabilities
    const uint32_t    ui32CapSize;
    const uint32_t    ui32Step;

} am_util_periph_power_header_t;

//*****************************************************************************
//
// Helper functions
//
//*****************************************************************************
extern void periphPowerInit(void);
extern void periphPowerExecute(void);
extern am_util_periph_power_header_t *pPeriphPowerConfig;

//*****************************************************************************
//
//  Attribute Macros to set the configuration structure at the beginning of the
//  first section of TCM (128K).
//
//*****************************************************************************
#if defined(keil) || defined(keil6)
#define PERIPH_POWER_CONFIG_LOCATION      __attribute__((section(".ARM.__at_0x20000000")))
#elif defined(iar)
#define PERIPH_POWER_CONFIG_LOCATION      __attribute__((section(".start_of_dtcm")))
#elif defined(gcc)
#define PERIPH_POWER_CONFIG_LOCATION      __attribute__((section(".start_of_dtcm")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_PERIPH_POWER_MAIN_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
