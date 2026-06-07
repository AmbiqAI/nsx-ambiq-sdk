//*****************************************************************************
//
//! @file am_util_test.h
//!
//! @brief Defines for test framework and testcases
//!
//! This file provides necessary macros for use in test programs
//!
//! @addtogroup test Test - Ambiq's Test Framework
//! @ingroup utils
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_UTIL_TEST_H
#define AM_UTIL_TEST_H

#include <stdint.h>

#ifdef MCU_VALIDATION_DEBUG_REG
// The MCU_VALIDATION_DEBUG_REG Register is organized as:
// LSB = Test Finished
// Most Significant 8 bits (bits 24-31) = Number of Tests
// While the test is not finished - bit 1-23 are a running counter
// Once the test is finished - bit 16-23 is NumFail, bit 8-15 are numIgnored

#ifdef MCU_VALIDATION_GPIO
#define AM_INIT_TEST()                                                  \
    do {                                                                \
        *((volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG)) = 0U; \
        am_hal_gpio_pinconfig(MCU_VALIDATION_GPIO, g_AM_VALIDATION_GPIO);\
        am_hal_gpio_state_write(MCU_VALIDATION_GPIO, AM_HAL_GPIO_OUTPUT_SET);\
    } while (0);
#define AM_UPDATE_TEST_PROGRESS()                                       \
    do {                                                                \
        volatile uint32_t *pDbg =                                       \
            (volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG); \
        uint32_t val1 = *pDbg;                                          \
        val1 = val1 + (1U << 1);                                        \
        *pDbg = val1;                                                   \
        am_hal_gpio_state_write(MCU_VALIDATION_GPIO, AM_HAL_GPIO_OUTPUT_TOGGLE);\
    } while (0);

#else

#define AM_INIT_TEST()                                                  \
    do {                                                                \
        *((volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG)) = 0U; \
    } while (0);
#define AM_UPDATE_TEST_PROGRESS()                                       \
    do {                                                                \
        volatile uint32_t *pDbg =                                       \
            (volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG); \
        uint32_t val1 = *pDbg;                                          \
        val1 = val1 + (1U << 1);                                        \
        *pDbg = val1;                                                   \
    } while (0);

#endif

#define AM_UPDATE_TESTNUM(numTest)                                      \
    do {                                                                \
        volatile uint32_t *pDbg =                                       \
            (volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG); \
        uint32_t val1 = *pDbg;                                          \
        val1 = (val1 & 0xFFFFFFU) |                                     \
               (((uint32_t)(numTest) & 0xFFU) << 24);                    \
        val1 &= ~(uint32_t)0x1U; /* in progress: clear completion LSB */ \
        *pDbg = val1;                                                   \
    } while (0);
#define AM_UPDATE_TEST_RESULT(numTest, numFail, numIgnored)             \
    do {                                                                \
        uint32_t val1 =                                                 \
            (((uint32_t)(numTest) & 0xFFU) << 24) |                     \
            (((uint32_t)(numFail) & 0xFFU) << 16) |                     \
            (((uint32_t)(numIgnored) & 0xFFU) << 8);                    \
        /* LS byte: only bit 0 = complete; clear bits 7:1 (old counter). */ \
        val1 = (val1 & 0xFFFFFF00U) | 0x01U;                            \
        *((volatile uint32_t *)(uintptr_t)(MCU_VALIDATION_DEBUG_REG)) = val1; \
    } while (0);
#else
// Dummy stubs
#define AM_INIT_TEST()
#define AM_UPDATE_TESTNUM(numTest)
#define AM_UPDATE_TEST_PROGRESS()
#define AM_UPDATE_TEST_RESULT(numTest, numFail, numIgnored)
#endif

#endif  // AM_UTIL_TEST_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

