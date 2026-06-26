//*****************************************************************************
//
//! @file startup_keil6.c
//!
//! @brief Definitions for the Apollo4 Plus vector table, interrupt handlers,
//! and reset handler for the Arm Compiler 6 (armclang) toolchain.
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
// This is the armclang (Arm Compiler 6) C startup for Apollo4 Plus.  It
// replaces the legacy armasm-syntax startup_armclang.s, which the armclang
// integrated (GNU-syntax) assembler cannot parse.  It follows the modern CMSIS
// scatter-loading model already used by the newer SoCs: the vector table is
// emitted into the "RESET" section, the initial stack pointer comes from the
// scatter file's ARM_LIB_STACK region, and __PROGRAM_START (__main) performs
// scatter-load before calling main().
//

#include "apollo4p.h"

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
extern void am_iomaster6_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_iomaster7_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_ctimer_isr(void)             __attribute ((weak, alias ("am_default_isr")));
extern void am_uart_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_uart1_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_uart2_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_uart3_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_adc_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi0_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi1_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_mspi2_isr(void)              __attribute ((weak, alias ("am_default_isr")));
extern void am_clkgen_isr(void)             __attribute ((weak, alias ("am_default_isr")));
extern void am_cryptosec_isr(void)          __attribute ((weak, alias ("am_default_isr")));
extern void am_sdio_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_usb_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_gpu_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_disp_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_dsi_isr(void)                __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr0_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr1_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr2_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr3_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr4_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr5_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr6_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimer_cmpr7_isr(void)       __attribute ((weak, alias ("am_default_isr")));
extern void am_stimerof_isr(void)           __attribute ((weak, alias ("am_default_isr")));
extern void am_audadc0_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_dspi2s0_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_dspi2s1_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_dspi2s2_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_dspi2s3_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_pdm0_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_pdm1_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_pdm2_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_pdm3_isr(void)               __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio0_001f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio0_203f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio0_405f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio0_607f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio1_001f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio1_203f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio1_405f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_gpio1_607f_isr(void)         __attribute ((weak, alias ("am_default_isr")));
extern void am_timer00_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer01_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer02_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer03_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer04_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer05_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer06_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer07_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer08_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer09_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer10_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer11_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer12_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer13_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer14_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_timer15_isr(void)            __attribute ((weak, alias ("am_default_isr")));
extern void am_cachecpu_isr(void)           __attribute ((weak, alias ("am_default_isr")));
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

// Apollo4 requires a 512-byte (128-word) aligned vector table: 16 core
// exceptions + 112 peripheral slots (IRQ 0..83 named, 84..111 reserved), so
// application code begins at offset 0x200.
extern const pfnVectorEntry_t __VECTOR_TABLE[128];
       const pfnVectorEntry_t __VECTOR_TABLE[128] __VECTOR_TABLE_ATTRIBUTE =
{
    (pfnVectorEntry_t)(&__INITIAL_SP),      // Initial Stack Pointer
    Reset_Handler,                          // The reset handler
    NMI_Handler,                            // The NMI handler
    HardFault_Handler,                      // The hard fault handler
    MemManage_Handler,                      // The MPU fault handler
    BusFault_Handler,                       // The bus fault handler
    UsageFault_Handler,                     // The usage fault handler
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
    am_watchdog_isr,                        //  1: Watchdog (WDT)
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
    am_iomaster6_isr,                       // 12: I/O Master 6 (I3C/I2C/SPI)
    am_iomaster7_isr,                       // 13: I/O Master 7 (I3C/I2C/SPI)
    am_ctimer_isr,                          // 14: OR of all timerX interrupts
    am_uart_isr,                            // 15: UART0
    am_uart1_isr,                           // 16: UART1
    am_uart2_isr,                           // 17: UART2
    am_uart3_isr,                           // 18: UART3
    am_adc_isr,                             // 19: ADC
    am_mspi0_isr,                           // 20: MSPI0
    am_mspi1_isr,                           // 21: MSPI1
    am_mspi2_isr,                           // 22: MSPI2
    am_clkgen_isr,                          // 23: ClkGen
    am_cryptosec_isr,                       // 24: Crypto Secure
    am_default_isr,                         // 25: Reserved
    am_sdio_isr,                            // 26: SDIO
    am_usb_isr,                             // 27: USB
    am_gpu_isr,                             // 28: GPU
    am_disp_isr,                            // 29: DISP
    am_dsi_isr,                             // 30: DSI
    am_default_isr,                         // 31: Reserved
    am_stimer_cmpr0_isr,                    // 32: System Timer Compare0
    am_stimer_cmpr1_isr,                    // 33: System Timer Compare1
    am_stimer_cmpr2_isr,                    // 34: System Timer Compare2
    am_stimer_cmpr3_isr,                    // 35: System Timer Compare3
    am_stimer_cmpr4_isr,                    // 36: System Timer Compare4
    am_stimer_cmpr5_isr,                    // 37: System Timer Compare5
    am_stimer_cmpr6_isr,                    // 38: System Timer Compare6
    am_stimer_cmpr7_isr,                    // 39: System Timer Compare7
    am_stimerof_isr,                        // 40: System Timer Cap Overflow
    am_default_isr,                         // 41: Reserved
    am_audadc0_isr,                         // 42: Audio ADC
    am_default_isr,                         // 43: Reserved
    am_dspi2s0_isr,                         // 44: I2S0
    am_dspi2s1_isr,                         // 45: I2S1
    am_dspi2s2_isr,                         // 46: I2S2
    am_dspi2s3_isr,                         // 47: I2S3
    am_pdm0_isr,                            // 48: PDM0
    am_pdm1_isr,                            // 49: PDM1
    am_pdm2_isr,                            // 50: PDM2
    am_pdm3_isr,                            // 51: PDM3
    am_default_isr,                         // 52: Reserved
    am_default_isr,                         // 53: Reserved
    am_default_isr,                         // 54: Reserved
    am_default_isr,                         // 55: Reserved
    am_gpio0_001f_isr,                      // 56: GPIO N0 pins  0-31
    am_gpio0_203f_isr,                      // 57: GPIO N0 pins 32-63
    am_gpio0_405f_isr,                      // 58: GPIO N0 pins 64-95
    am_gpio0_607f_isr,                      // 59: GPIO N0 pins 96-104, virtual 105-127
    am_gpio1_001f_isr,                      // 60: GPIO N1 pins  0-31
    am_gpio1_203f_isr,                      // 61: GPIO N1 pins 32-63
    am_gpio1_405f_isr,                      // 62: GPIO N1 pins 64-95
    am_gpio1_607f_isr,                      // 63: GPIO N1 pins 96-104, virtual 105-127
    am_default_isr,                         // 64: Reserved
    am_default_isr,                         // 65: Reserved
    am_default_isr,                         // 66: Reserved
    am_timer00_isr,                         // 67: timer0
    am_timer01_isr,                         // 68: timer1
    am_timer02_isr,                         // 69: timer2
    am_timer03_isr,                         // 70: timer3
    am_timer04_isr,                         // 71: timer4
    am_timer05_isr,                         // 72: timer5
    am_timer06_isr,                         // 73: timer6
    am_timer07_isr,                         // 74: timer7
    am_timer08_isr,                         // 75: timer8
    am_timer09_isr,                         // 76: timer9
    am_timer10_isr,                         // 77: timer10
    am_timer11_isr,                         // 78: timer11
    am_timer12_isr,                         // 79: timer12
    am_timer13_isr,                         // 80: timer13
    am_timer14_isr,                         // 81: timer14
    am_timer15_isr,                         // 82: timer15
    am_cachecpu_isr,                        // 83: CPU cache

    //
    // Patchable area - pads the vector table to 128 entries (16 core + 112
    // peripheral) so that code begins at offset 0x200.
    //
    0,                                      //  84
    0,                                      //  85
    0,                                      //  86
    0,                                      //  87
    0,                                      //  88
    0,                                      //  89
    0,                                      //  90
    0,                                      //  91
    0,                                      //  92
    0,                                      //  93
    0,                                      //  94
    0,                                      //  95
    0,                                      //  96
    0,                                      //  97
    0,                                      //  98
    0,                                      //  99
    0,                                      // 100
    0,                                      // 101
    0,                                      // 102
    0,                                      // 103
    0,                                      // 104
    0,                                      // 105
    0,                                      // 106
    0,                                      // 107
    0,                                      // 108
    0,                                      // 109
    0,                                      // 110
    0                                       // 111
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
