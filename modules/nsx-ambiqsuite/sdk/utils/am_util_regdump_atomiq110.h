//*****************************************************************************
//
//! @file am_util_regdump_atomiq110.h
//!
//! @brief Functions to aid register dumping and debugging (Atomiq110 specific)
//!
//! @addtogroup regdump_atomiq110_utils Register Dump Functionality - Atomiq110
//! @ingroup utils
//! @{
//!
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#if !defined(AM_UTIL_REGDUMP_ATOMIQ110_H)
#define AM_UTIL_REGDUMP_ATOMIQ110_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "am_util_stdio.h"
#include "am_bsp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//*****************************************************************************
//
//! @brief Configuration option for full register dump functionality
//!
//! Define AM_UTIL_REGDUMP_ALL to include all register blocks in the
//! am_util_get_all_registers() dump. Otherwise, define one or more
//! block macros to include only those register ranges (saves code and
//! data when only specific blocks are needed).
//!
//! Per-block macros (match register map block names):
//!     AM_UTIL_REGDUMP_ADC, AM_UTIL_REGDUMP_AXI, AM_UTIL_REGDUMP_CLKGEN,
//!     AM_UTIL_REGDUMP_CRM, AM_UTIL_REGDUMP_CRYPTO, AM_UTIL_REGDUMP_DBI_DSI_BRIDGE,
//!     AM_UTIL_REGDUMP_DC, AM_UTIL_REGDUMP_DMEA, AM_UTIL_REGDUMP_DPI_GEAR_BOX,
//!     AM_UTIL_REGDUMP_DSI, AM_UTIL_REGDUMP_DSP_IPB_I2S, AM_UTIL_REGDUMP_DSP_IPB_NO_ASRC,
//!     AM_UTIL_REGDUMP_EMMC, AM_UTIL_REGDUMP_FPIO, AM_UTIL_REGDUMP_GPIO,
//!     AM_UTIL_REGDUMP_GPU, AM_UTIL_REGDUMP_HBLRAM, AM_UTIL_REGDUMP_I2S_NO_ASRC,
//!     AM_UTIL_REGDUMP_I3C, AM_UTIL_REGDUMP_INFO0, AM_UTIL_REGDUMP_INFO1,
//!     AM_UTIL_REGDUMP_IOM, AM_UTIL_REGDUMP_IOSLAVEFD, AM_UTIL_REGDUMP_ITM,
//!     AM_UTIL_REGDUMP_LAU, AM_UTIL_REGDUMP_LPADC, AM_UTIL_REGDUMP_MCUCTRL,
//!     AM_UTIL_REGDUMP_MMU, AM_UTIL_REGDUMP_NPU, AM_UTIL_REGDUMP_NVIC,
//!     AM_UTIL_REGDUMP_OTP, AM_UTIL_REGDUMP_PDM, AM_UTIL_REGDUMP_PWRCTRL,
//!     AM_UTIL_REGDUMP_RSTGEN, AM_UTIL_REGDUMP_RTC, AM_UTIL_REGDUMP_SCM,
//!     AM_UTIL_REGDUMP_SDIO, AM_UTIL_REGDUMP_SECURE_RTC, AM_UTIL_REGDUMP_SECURE_STIMER,
//!     AM_UTIL_REGDUMP_SECURE_TIMER, AM_UTIL_REGDUMP_SECURE_WDT, AM_UTIL_REGDUMP_SECURITY,
//!     AM_UTIL_REGDUMP_SSC, AM_UTIL_REGDUMP_STIMER, AM_UTIL_REGDUMP_SYSCTRL,
//!     AM_UTIL_REGDUMP_SYSTICK, AM_UTIL_REGDUMP_TIMER, AM_UTIL_REGDUMP_TPIU,
//!     AM_UTIL_REGDUMP_UART, AM_UTIL_REGDUMP_USB, AM_UTIL_REGDUMP_USBPHY,
//!     AM_UTIL_REGDUMP_VCOMP, AM_UTIL_REGDUMP_WDT, AM_UTIL_REGDUMP_XSPIMC,
//!     AM_UTIL_REGDUMP_XSPIS, AM_UTIL_REGDUMP_XSPISLV
//!
//! Example - dump only RSTGEN and CLKGEN:
//!     #define AM_UTIL_REGDUMP_RSTGEN
//!     #define AM_UTIL_REGDUMP_CLKGEN
//!
//! Example - dump all blocks:
//!     #define AM_UTIL_REGDUMP_ALL
//!
//*****************************************************************************
#if defined(AM_UTIL_REGDUMP_ALL)
#define AM_UTIL_REGDUMP_ANY
#define AM_UTIL_REGDUMP_ADC
#define AM_UTIL_REGDUMP_AXI
#define AM_UTIL_REGDUMP_CLKGEN
#define AM_UTIL_REGDUMP_CRM
#define AM_UTIL_REGDUMP_CRYPTO
#define AM_UTIL_REGDUMP_DBI_DSI_BRIDGE
#define AM_UTIL_REGDUMP_DC
#define AM_UTIL_REGDUMP_DMEA
#define AM_UTIL_REGDUMP_DPI_GEAR_BOX
#define AM_UTIL_REGDUMP_DSI
#define AM_UTIL_REGDUMP_DSP_IPB_I2S
#define AM_UTIL_REGDUMP_DSP_IPB_NO_ASRC
#define AM_UTIL_REGDUMP_EMMC
#define AM_UTIL_REGDUMP_FPIO
#define AM_UTIL_REGDUMP_GPIO
#define AM_UTIL_REGDUMP_GPU
#define AM_UTIL_REGDUMP_HBLRAM
#define AM_UTIL_REGDUMP_I2S_NO_ASRC
#define AM_UTIL_REGDUMP_I3C
#define AM_UTIL_REGDUMP_INFO0
#define AM_UTIL_REGDUMP_INFO1
#define AM_UTIL_REGDUMP_IOM
#define AM_UTIL_REGDUMP_IOSLAVEFD
#define AM_UTIL_REGDUMP_ITM
#define AM_UTIL_REGDUMP_LAU
#define AM_UTIL_REGDUMP_LPADC
#define AM_UTIL_REGDUMP_MCUCTRL
#define AM_UTIL_REGDUMP_MMU
#define AM_UTIL_REGDUMP_NPU
#define AM_UTIL_REGDUMP_NVIC
#define AM_UTIL_REGDUMP_OTP
#define AM_UTIL_REGDUMP_PDM
#define AM_UTIL_REGDUMP_PWRCTRL
#define AM_UTIL_REGDUMP_RSTGEN
#define AM_UTIL_REGDUMP_RTC
#define AM_UTIL_REGDUMP_SCM
#define AM_UTIL_REGDUMP_SDIO
#define AM_UTIL_REGDUMP_SECURE_RTC
#define AM_UTIL_REGDUMP_SECURE_STIMER
#define AM_UTIL_REGDUMP_SECURE_TIMER
#define AM_UTIL_REGDUMP_SECURE_WDT
#define AM_UTIL_REGDUMP_SECURITY
#define AM_UTIL_REGDUMP_SSC
#define AM_UTIL_REGDUMP_STIMER
#define AM_UTIL_REGDUMP_SYSCTRL
#define AM_UTIL_REGDUMP_SYSTICK
#define AM_UTIL_REGDUMP_TIMER
#define AM_UTIL_REGDUMP_TPIU
#define AM_UTIL_REGDUMP_UART
#define AM_UTIL_REGDUMP_USB
#define AM_UTIL_REGDUMP_USBPHY
#define AM_UTIL_REGDUMP_VCOMP
#define AM_UTIL_REGDUMP_WDT
#define AM_UTIL_REGDUMP_XSPIMC
#define AM_UTIL_REGDUMP_XSPIS
#define AM_UTIL_REGDUMP_XSPISLV
#else
#if defined(AM_UTIL_REGDUMP_ADC) \
    || defined(AM_UTIL_REGDUMP_AXI) \
    || defined(AM_UTIL_REGDUMP_CLKGEN) \
    || defined(AM_UTIL_REGDUMP_CRM) \
    || defined(AM_UTIL_REGDUMP_CRYPTO) \
    || defined(AM_UTIL_REGDUMP_DBI_DSI_BRIDGE) \
    || defined(AM_UTIL_REGDUMP_DC) \
    || defined(AM_UTIL_REGDUMP_DMEA) \
    || defined(AM_UTIL_REGDUMP_DPI_GEAR_BOX) \
    || defined(AM_UTIL_REGDUMP_DSI) \
    || defined(AM_UTIL_REGDUMP_DSP_IPB_I2S) \
    || defined(AM_UTIL_REGDUMP_DSP_IPB_NO_ASRC) \
    || defined(AM_UTIL_REGDUMP_EMMC) \
    || defined(AM_UTIL_REGDUMP_FPIO) \
    || defined(AM_UTIL_REGDUMP_GPIO) \
    || defined(AM_UTIL_REGDUMP_GPU) \
    || defined(AM_UTIL_REGDUMP_HBLRAM) \
    || defined(AM_UTIL_REGDUMP_I2S_NO_ASRC) \
    || defined(AM_UTIL_REGDUMP_I3C) \
    || defined(AM_UTIL_REGDUMP_INFO0) \
    || defined(AM_UTIL_REGDUMP_INFO1) \
    || defined(AM_UTIL_REGDUMP_IOM) \
    || defined(AM_UTIL_REGDUMP_IOSLAVEFD) \
    || defined(AM_UTIL_REGDUMP_ITM) \
    || defined(AM_UTIL_REGDUMP_LAU) \
    || defined(AM_UTIL_REGDUMP_LPADC) \
    || defined(AM_UTIL_REGDUMP_MCUCTRL) \
    || defined(AM_UTIL_REGDUMP_MMU) \
    || defined(AM_UTIL_REGDUMP_NPU) \
    || defined(AM_UTIL_REGDUMP_NVIC) \
    || defined(AM_UTIL_REGDUMP_OTP) \
    || defined(AM_UTIL_REGDUMP_PDM) \
    || defined(AM_UTIL_REGDUMP_PWRCTRL) \
    || defined(AM_UTIL_REGDUMP_RSTGEN) \
    || defined(AM_UTIL_REGDUMP_RTC) \
    || defined(AM_UTIL_REGDUMP_SCM) \
    || defined(AM_UTIL_REGDUMP_SDIO) \
    || defined(AM_UTIL_REGDUMP_SECURE_RTC) \
    || defined(AM_UTIL_REGDUMP_SECURE_STIMER) \
    || defined(AM_UTIL_REGDUMP_SECURE_TIMER) \
    || defined(AM_UTIL_REGDUMP_SECURE_WDT) \
    || defined(AM_UTIL_REGDUMP_SECURITY) \
    || defined(AM_UTIL_REGDUMP_SSC) \
    || defined(AM_UTIL_REGDUMP_STIMER) \
    || defined(AM_UTIL_REGDUMP_SYSCTRL) \
    || defined(AM_UTIL_REGDUMP_SYSTICK) \
    || defined(AM_UTIL_REGDUMP_TIMER) \
    || defined(AM_UTIL_REGDUMP_TPIU) \
    || defined(AM_UTIL_REGDUMP_UART) \
    || defined(AM_UTIL_REGDUMP_USB) \
    || defined(AM_UTIL_REGDUMP_USBPHY) \
    || defined(AM_UTIL_REGDUMP_VCOMP) \
    || defined(AM_UTIL_REGDUMP_WDT) \
    || defined(AM_UTIL_REGDUMP_XSPIMC) \
    || defined(AM_UTIL_REGDUMP_XSPIS) \
    || defined(AM_UTIL_REGDUMP_XSPISLV)
#define AM_UTIL_REGDUMP_ANY
#endif
#endif

#if defined(AM_UTIL_REGDUMP_ANY)

//*****************************************************************************
//
//! @brief Register dump data structures and definitions (when AM_UTIL_REGDUMP_ANY)
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Power status bit definitions for DEVPWRSTATUS0 register
//!
//! These defines map to the power status bits in the DEVPWRSTATUS0 register
//! (0x4000C008) to determine which peripheral domains are powered and accessible.
//
//*****************************************************************************
#define AM_UTIL_REGDUMP_PWRSTIOM0            (1 << 1)    //!< Power status IO Master 0
#define AM_UTIL_REGDUMP_PWRSTIOM1            (1 << 2)    //!< Power status IO Master 1
#define AM_UTIL_REGDUMP_PWRSTIOM2            (1 << 3)    //!< Power status IO Master 2
#define AM_UTIL_REGDUMP_PWRSTIOM3            (1 << 4)    //!< Power status IO Master 3
#define AM_UTIL_REGDUMP_PWRSTUART0           (1 << 9)    //!< Power status UART Controller 0
#define AM_UTIL_REGDUMP_PWRSTUART1           (1 << 10)   //!< Power status UART Controller 1
#define AM_UTIL_REGDUMP_PWRSTADC             (1 << 13)   //!< Power status ADC Digital Controller
#define AM_UTIL_REGDUMP_PWRSTXSPI0           (1 << 14)   //!< Power status XSPI Controller 0
#define AM_UTIL_REGDUMP_PWRSTXSPI1           (1 << 15)   //!< Power status XSPI Controller 1
#define AM_UTIL_REGDUMP_PWRSTXSPI2           (1 << 16)   //!< Power status XSPI Controller 2
#define AM_UTIL_REGDUMP_PWRSTEMMC            (1 << 17)   //!< Power status EMMC controller
#define AM_UTIL_REGDUMP_PWRSTGFX             (1 << 18)   //!< Power status GFX controller
#define AM_UTIL_REGDUMP_PWRSTDISP            (1 << 19)   //!< Power status DISP controller
#define AM_UTIL_REGDUMP_PWRSTDISPPHY         (1 << 20)   //!< Power status DISP PHY
#define AM_UTIL_REGDUMP_PWRSTCRYPTO          (1 << 21)   //!< Power status CRYPTO module
#define AM_UTIL_REGDUMP_PWRSTSDIO0           (1 << 22)   //!< Power status SDIO0 controller
#define AM_UTIL_REGDUMP_PWRSTUSB             (1 << 24)   //!< Power status USB controller
#define AM_UTIL_REGDUMP_PWRSTUSBPHY          (1 << 25)   //!< Power status USB PHY
#define AM_UTIL_REGDUMP_PWRSTDBG             (1 << 26)   //!< Power status DBG subsystem
#define AM_UTIL_REGDUMP_PWRSTOTP             (1 << 27)   //!< Power status OTP
#define AM_UTIL_REGDUMP_PWRSTIOSFD0          (1 << 28)   //!< Power status IO FD Slave 0
#define AM_UTIL_REGDUMP_PWRSTIOSFD1          (1 << 29)   //!< Power status IO FD Slave 1
#define AM_UTIL_REGDUMP_PWRSTI3C0            (1 << 30)   //!< Power status I3C0
#define AM_UTIL_REGDUMP_PWRSTLPADC           (1 << 31)   //!< Power status LPADC Digital Controller

//*****************************************************************************
//
//! @brief Power status bit definitions for DEVPWRSTATUS1 register
//!
//! These defines map to the power status bits in the DEVPWRSTATUS1 register
//! (0x4000C03C) to determine which peripheral domains are powered and accessible.
//
//*****************************************************************************
#define AM_UTIL_REGDUMP_PWRSTIOM4            (1 << 0)    //!< Power status IO Master 4
#define AM_UTIL_REGDUMP_PWRSTIOM5            (1 << 1)    //!< Power status IO Master 5
#define AM_UTIL_REGDUMP_PWRSTIOM6            (1 << 2)    //!< Power status IO Master 6
#define AM_UTIL_REGDUMP_PWRSTIOM7            (1 << 3)    //!< Power status IO Master 7
#define AM_UTIL_REGDUMP_PWRSTIOM8            (1 << 4)    //!< Power status IO Master 8
#define AM_UTIL_REGDUMP_PWRSTIOM9            (1 << 5)    //!< Power status IO Master 9
#define AM_UTIL_REGDUMP_PWRSTIOM10           (1 << 6)    //!< Power status IO Master 10
#define AM_UTIL_REGDUMP_PWRSTIOM11           (1 << 7)    //!< Power status IO Master 11
#define AM_UTIL_REGDUMP_PWRSTI3C1            (1 << 8)    //!< Power status I3C1
#define AM_UTIL_REGDUMP_PWRSTI3C2            (1 << 9)    //!< Power status I3C2
#define AM_UTIL_REGDUMP_PWRSTUART2           (1 << 10)   //!< Power status UART Controller 2
#define AM_UTIL_REGDUMP_PWRSTUART3           (1 << 11)   //!< Power status UART Controller 3
#define AM_UTIL_REGDUMP_PWRSTUART4           (1 << 12)   //!< Power status UART Controller 4
#define AM_UTIL_REGDUMP_PWRSTUART5           (1 << 13)   //!< Power status UART Controller 5
#define AM_UTIL_REGDUMP_PWRSTNPU             (1 << 14)   //!< Power status NPU
#define AM_UTIL_REGDUMP_PWRSTMMU             (1 << 15)   //!< Power status MMU
#define AM_UTIL_REGDUMP_PWRSTSYSPLL          (1 << 17)   //!< Power status SYSPLL
#define AM_UTIL_REGDUMP_PWRSTMEMPLL          (1 << 18)   //!< Power status MEMPLL
#define AM_UTIL_REGDUMP_PWRSTEMMCPHY         (1 << 19)   //!< Power status EMMCPHY
#define AM_UTIL_REGDUMP_PWRSTHBLRAM          (1 << 20)   //!< Power status HBLRAM
#define AM_UTIL_REGDUMP_PWRSTXSPISLV         (1 << 21)   //!< Power status XSPISLV

//*****************************************************************************
//
//! @brief Audio subsystem power status bit definitions for AUDSSPWRSTATUS register
//!
//! These defines map to the power status bits in the AUDSSPWRSTATUS register
//! (0x4000C010) to determine which audio peripherals are powered and accessible.
//
//*****************************************************************************
#define AM_UTIL_REGDUMP_PWRSTPDM0            (1 << 2)    //!< Power status audio subsystem PDM0 domain
#define AM_UTIL_REGDUMP_PWRSTPDM1            (1 << 3)    //!< Power status audio subsystem PDM1 domain
#define AM_UTIL_REGDUMP_PWRSTPDM2            (1 << 4)    //!< Power status audio subsystem PDM2 domain
#define AM_UTIL_REGDUMP_PWRSTI2S0            (1 << 6)    //!< Power status audio subsystem I2S0 domain
#define AM_UTIL_REGDUMP_PWRSTI2S1            (1 << 7)    //!< Power status audio subsystem I2S1 domain

//*****************************************************************************
//
//! @brief ADC power status bit definitions for ADCSTATUS register
//!
//! These defines map to the power status bits in the ADCSTATUS register
//! (0x4000C194) to determine which ADC blocks are powered and accessible.
//
//*****************************************************************************
#define AM_UTIL_REGDUMP_ADCPWD               (1 << 0)    //!< ADC powered down

//*****************************************************************************
//
//! @brief Structure to hold register group information
//!
//! This structure contains the starting address and count for each contiguous
//! register group found in the Atomiq110 register map.
//
//*****************************************************************************
typedef struct
{
    uint32_t start_address;    //!< Starting address of the register group
    uint32_t count;            //!< Number of registers in the group
}
am_util_register_group_t;

//*****************************************************************************
//
//! @brief Array of all contiguous register groups
//!
//! This array contains all the register group definitions for efficient
//! bulk reading of Atomiq110 registers.
//
//*****************************************************************************
extern const am_util_register_group_t am_util_register_groups[];

//*****************************************************************************
//
//! @brief Total number of register groups
//!
//! This is calculated dynamically in the source file using sizeof().
//! The actual count is determined at compile time based on the array size.
//
//*****************************************************************************
extern const uint32_t am_util_register_group_count;

//*****************************************************************************
//
//! @brief Dump all available registers via UART
//!
//! @param void
//!
//! This function iterates through all register groups and prints each
//! register address and value using am_util_stdio_printf(). It processes
//! registers in batches to avoid buffer overflow.
//!
//! @note - before use this function, make sure the UART or SWO is enabled
//!
//! @returns None
//
//*****************************************************************************
extern void am_util_get_all_registers(void);

#endif // AM_UTIL_REGDUMP_ANY

//*****************************************************************************
//
//! @brief Dump registers within a specified address range via UART
//!
//! @param ui32StartAddr - Starting address of the register range (must be 4-byte aligned)
//! @param ui32EndAddr   - Ending address of the register range (inclusive, must be 4-byte aligned)
//!
//! This function reads and prints all contiguous 32-bit registers from the
//! start address to the end address (inclusive). It uses AM_REGVAL() macro
//! for direct register access.
//!
//! @note - Before using this function, make sure the UART or SWO is enabled
//! @note - Both addresses must be 4-byte aligned
//! @note - End address must be >= start address
//! @note - Caller is responsible for ensuring the address range is valid
//!         and the peripherals are powered before calling this function
//!
//! @returns None
//
//*****************************************************************************
extern void am_util_get_registers(uint32_t ui32StartAddr, uint32_t ui32EndAddr);

//*****************************************************************************
//
//! @brief Dump a specified number of registers starting from an address via UART
//!
//! @param ui32StartAddr - Starting address of the register range (must be 4-byte aligned)
//! @param ui32Count     - Number of 32-bit registers to read
//!
//! This function reads and prints ui32Count contiguous 32-bit registers starting
//! from the start address. It uses AM_REGVAL() macro for direct register access.
//!
//! @note - Before using this function, make sure the UART or SWO is enabled
//! @note - Start address must be 4-byte aligned
//! @note - Caller is responsible for ensuring the address range is valid
//!         and the peripherals are powered before calling this function
//!
//! @returns None
//
//*****************************************************************************
extern void am_util_get_n_registers(uint32_t ui32StartAddr, uint32_t ui32Count);

#if defined(__cplusplus)
}
#endif

#endif // AM_UTIL_REGDUMP_ATOMIQ110_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
