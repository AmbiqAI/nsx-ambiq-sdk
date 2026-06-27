//*****************************************************************************
//
//! @file startup_keil6.c
//!
//! @brief Definitions for the Apollo3 Blue Plus vector table, interrupt
//! handlers, and reset handler for the Arm Compiler 6 (armclang) toolchain.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2024, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//
// This is the armclang (Arm Compiler 6) C startup for Apollo3 Blue Plus.  It
// replaces the legacy armasm-syntax startup_keil6.s, which the armclang
// integrated (GNU-syntax) assembler cannot parse.  It follows the modern CMSIS
// scatter-loading model already used by the newer SoCs: the vector table is
// emitted into the "RESET" section, the initial stack pointer comes from the
// scatter file's ARM_LIB_STACK region, and __PROGRAM_START (__main) performs
// scatter-load before calling main().
//

#include "apollo3p.h"

/*----------------------------------------------------------------------------
  External References
 *----------------------------------------------------------------------------*/
extern uint32_t __INITIAL_SP;

extern __NO_RETURN void __PROGRAM_START(void);

//*****************************************************************************
//
// Forward declaration of interrupt handlers.
//
//*****************************************************************************
extern void Reset_Handler(void)             __attribute ((weak));
extern void NMI_Handler(void)               __attribute ((weak, alias ("am_default_isr")));
extern void HardFault_Handler(void)         __attribute ((weak));
extern void MemManage_Handler(void)         __attribute ((weak, alias ("HardFault_Handler")));
extern void BusFault_Handler(void)          __attribute ((weak, alias ("HardFault_Handler")));
extern void UsageFault_Handler(void)        __attribute ((weak, alias ("HardFault_Handler")));
extern void SVC_Handler(void)               __attribute ((weak, alias ("am_default_isr")));
extern void DebugMon_Handler(void)          __attribute ((weak, alias ("am_default_isr")));
extern void PendSV_Handler(void)            __attribute ((weak, alias ("am_default_isr")));
extern void SysTick_Handler(void)           __attribute ((weak, alias ("am_default_isr")));

extern void am_brownout_isr(void)           __attribute ((weak, alias ("am_default_isr")));
extern void am_watchdog_isr(void)           __attribute ((weak, alias ("am_default_isr")));
extern void am_rtc_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_vcomp_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_ioslave_ios_isr(void)        __attribute ((weak, alias ("am_default_isr")));
extern void am_ioslave_acc_isr(void)        __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster0_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster1_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster2_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster3_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster4_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster5_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_ble_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_ctimer_isr(void)             __attribute ((weak, alias ("am_default_isr")));
extern void am_uart_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_uart1_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_scard_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_adc_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_pdm0_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi0_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_software0_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_isr(void)             __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr0_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr1_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr2_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr3_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr4_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr5_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr6_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr7_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_clkgen_isr(void)             __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi1_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi2_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_default_isr(void)            __attribute ((weak));

/*----------------------------------------------------------------------------
  Exception / Interrupt Vector table
 *----------------------------------------------------------------------------*/

#if defined ( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

// Vector table entry type.  CMSIS does not provide VECTOR_TABLE_Type for the
// Cortex-M4 device headers, so define a local function-pointer type (the first
// entry holds the initial stack pointer, cast to this type).
typedef void (*pfnVectorEntry_t)(void);

extern const pfnVectorEntry_t __VECTOR_TABLE[64];
       const pfnVectorEntry_t __VECTOR_TABLE[64] __VECTOR_TABLE_ATTRIBUTE =
{
    (pfnVectorEntry_t)(&__INITIAL_SP),      // Initial Stack Pointer
    Reset_Handler,                          // The reset handler
    NMI_Handler,                            // The NMI handler
    HardFault_Handler,                      // The hard fault handler
    MemManage_Handler,                      // The MemManage_Handler
    BusFault_Handler,                       // The BusFault_Handler
    UsageFault_Handler,                     // The UsageFault_Handler
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    SVC_Handler,                            // SVCall handler
    DebugMon_Handler,                       // Debug monitor handler
    0,                                      // Reserved
    PendSV_Handler,                         // The PendSV handler
    SysTick_Handler,                        // The SysTick handler

    //
    // Peripheral Interrupts
    //
    am_brownout_isr,                        //  0: Brownout (rstgen)
    am_watchdog_isr,                        //  1: Watchdog
    am_rtc_isr,                             //  2: RTC
    am_vcomp_isr,                           //  3: Voltage Comparator
    am_ioslave_ios_isr,                     //  4: I/O Slave general
    am_ioslave_acc_isr,                     //  5: I/O Slave access
    am_iomaster0_isr,                       //  6: I/O Master 0
    am_iomaster1_isr,                       //  7: I/O Master 1
    am_iomaster2_isr,                       //  8: I/O Master 2
    am_iomaster3_isr,                       //  9: I/O Master 3
    am_iomaster4_isr,                       // 10: I/O Master 4
    am_iomaster5_isr,                       // 11: I/O Master 5
    am_ble_isr,                             // 12: BLEIF
    am_gpio_isr,                            // 13: GPIO
    am_ctimer_isr,                          // 14: CTIMER
    am_uart_isr,                            // 15: UART0
    am_uart1_isr,                           // 16: UART1
    am_scard_isr,                           // 17: SCARD
    am_adc_isr,                             // 18: ADC
    am_pdm0_isr,                            // 19: PDM
    am_mspi0_isr,                           // 20: MSPI0
    am_software0_isr,                       // 21: SOFTWARE0
    am_stimer_isr,                          // 22: SYSTEM TIMER
    am_stimer_cmpr0_isr,                    // 23: SYSTEM TIMER COMPARE0
    am_stimer_cmpr1_isr,                    // 24: SYSTEM TIMER COMPARE1
    am_stimer_cmpr2_isr,                    // 25: SYSTEM TIMER COMPARE2
    am_stimer_cmpr3_isr,                    // 26: SYSTEM TIMER COMPARE3
    am_stimer_cmpr4_isr,                    // 27: SYSTEM TIMER COMPARE4
    am_stimer_cmpr5_isr,                    // 28: SYSTEM TIMER COMPARE5
    am_stimer_cmpr6_isr,                    // 29: SYSTEM TIMER COMPARE6
    am_stimer_cmpr7_isr,                    // 30: SYSTEM TIMER COMPARE7
    am_clkgen_isr,                          // 31: CLKGEN
    am_mspi1_isr,                           // 32: MSPI1
    am_mspi2_isr,                           // 33: MSPI2

    //
    // Patchable area - pads the vector table to 64 entries (16 core + 48
    // peripheral) so that code begins at offset 0x100.
    //
    0,                                      // 34
    0,                                      // 35
    0,                                      // 36
    0,                                      // 37
    0,                                      // 38
    0,                                      // 39
    0,                                      // 40
    0,                                      // 41
    0,                                      // 42
    0,                                      // 43
    0,                                      // 44
    0,                                      // 45
    0,                                      // 46
    0                                       // 47
};

#if defined ( __GNUC__ )
#pragma GCC diagnostic pop
#endif

/*----------------------------------------------------------------------------
  Reset Handler called on controller reset
 *----------------------------------------------------------------------------*/
__NO_RETURN void Reset_Handler(void)
{
    //
    // Enable the FPU (full access to CP10 and CP11), matching the legacy
    // armasm startup.  __PROGRAM_START (__main) then performs scatter-load and
    // transfers control to main().
    //
    SCB->CPACR |= ((3U << 10U * 2U) | (3U << 11U * 2U));
    __DSB();
    __ISB();

    // The Apollo secure-boot handoff may leave global interrupts masked
    // (PRIMASK=1).  Restore the architectural app-entry expectation before user
    // code runs, matching the GCC startup's `cpsie i`.  Without this, interrupt-
    // driven peripherals never service their IRQs.
    __enable_irq();

    __PROGRAM_START();
}

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

/*----------------------------------------------------------------------------
  Hard Fault Handler
 *----------------------------------------------------------------------------*/
void HardFault_Handler(void)
{
    while (1);
}

/*----------------------------------------------------------------------------
  Default Handler for Exceptions / Interrupts
 *----------------------------------------------------------------------------*/
void am_default_isr(void)
{
    while (1);
}

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic pop
#endif
