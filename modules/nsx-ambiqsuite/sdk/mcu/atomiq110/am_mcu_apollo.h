//*****************************************************************************
//
//! @file am_mcu_apollo.h
//!
//! @brief Top Include for atomiq110 class devices.
//!
//! This file provides all the includes necessary for an atomiq device.
//!
//! @addtogroup hal mcu
//
//! @defgroup atomiq110_hal atomiq110
//! @ingroup hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_MCU_ATOMIQ_H
#define AM_MCU_ATOMIQ_H

//*****************************************************************************
//
// Arm architecture intrinsics and options
//  Security extensions
//  Vector extension
//
//*****************************************************************************
#include <arm_cmse.h>               // Include ARM Security Extensions

#if   (__ARM_FEATURE_MVE & 3) == 3  // MVE integer and floating point intrinsics available
#include <arm_mve.h>                // Include M-Profile Vector Extension aka Helium
#elif (__ARM_FEATURE_MVE & 1) == 1  // MVE integer intrinsics available
#include <arm_mve.h>                // Include M-Profile Vector Extension aka Helium
#else
// Compiler options for vector extensions are not enabled.
#ifndef AM_HAL_SKIP_NO_MVE_WARNING
// #### INTERNAL BEGIN ####
#warning TODO FIXME MVE intrinsics not available
// #### INTERNAL END ####
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// FPGA-specific defines.
//
//*****************************************************************************
//
// Specify that the Atomiq11x FPGA is in use.
// If defined, it is assumed to designate the target SOF frequency in MHz.
// For example, if the SOF frequency is designated as 12MHz or 48MHz, then
// ATOMIQ11X_FPGA should be set to the value of 12 or 48, respectively.
// If the value is changed, the HAL should be rebuilt.
// This define used to support FPGA-specific differences in the HAL as well
//  as modify timings within the HAL for the speed of the FPGA.
//
// Some notes about FPGA target speeds.
// - FPGA HFRC speeds are typically based on divisions of 50MHz as opposed
//   to 96MHz, 48MHz, etc.
// - HFRC can be measured on designated pins by doing 2 things:
//   1. Configuring CLKGEN->CLKOUT with CLKGEN_CLKOUT_CKSEL_HFRC and
//      CLKGEN_CLKOUT_CKEN_EN.
//   2. Configuring the GPIO with FNCSEL=CLKOUT.
//

#warning "am_mcu_apollo.h: ATOMIQ11X_FPGA is defined here. Must be removed for silicon."
//
// It was defined here (as opposed to config.ini) for those instances when the
// HAL is pulled into a debug (IDE) environment and needs to be defined there.
// While defining it in config.ini is preferred, it does not work in the IDE.
//
// As of Atomiq110 Drop6 c5/26/26, the default core frequency is 25MHz.
//
#define ATOMIQ11X_FPGA                 25   // FPGA SOF target frequency (in MHz)

//*****************************************************************************
//
//! AM_PART_ATOMIQ11X_API indicates that this device uses the Atomiq11x API.
//
//*****************************************************************************
#define AM_PART_ATOMIQ11X_API     1

//*****************************************************************************
//
//! Define AM_CMSIS_REGS to indicate that CMSIS registers are supported.
//
//*****************************************************************************
#define AM_CMSIS_REGS           1

//*****************************************************************************
//
// C99
//
//*****************************************************************************
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//*****************************************************************************
//
// Atomiq CMSIS peripheral registers
//
//*****************************************************************************
#include "atomiq110.h"

//*****************************************************************************
//
// Global HAL
//
//*****************************************************************************
//
// Define this macro to disable and remove parameter validation in functions
// throughout the HAL.
//
//#define AM_HAL_DISABLE_API_VALIDATION

//*****************************************************************************
//
// Registers
//
//*****************************************************************************
#include "regs/am_reg_base_addresses.h"
#include "regs/am_reg_macros.h"
#include "regs/am_reg.h"
#include "regs/am_reg_jedec.h"

//*****************************************************************************
//
// HAL
//
//*****************************************************************************
#include "hal/am_hal_global.h"
#include "hal/am_hal_pin.h"
#include "hal/am_hal_status.h"
#include "hal/am_hal_sysctrl.h"

//
// HAL MCU includes
//
#include "hal/mcu/am_hal_bootrom_helper.h"
#include "hal/mcu/am_hal_cachectrl.h"
#include "hal/mcu/am_hal_clkgen.h"
#include "hal/mcu/am_hal_syspll.h"
#include "hal/am_hal_clkmgr.h"
#include "hal/am_hal_dme.h"
#include "hal/mcu/am_hal_card_host.h"
#include "hal/mcu/am_hal_card.h"
// #include "hal/mcu/am_hal_clkgen.h"
#include "hal/mcu/am_hal_cmdq.h"
#include "hal/mcu/am_hal_debug.h"
#ifdef NEMA_PLATFORM
#include "hal/mcu/am_hal_dc.h"
//#include "hal/mcu/am_hal_dsi.h"
#endif
#include "hal/mcu/am_hal_lau.h"
#include "hal/mcu/am_hal_i3c.h"
#include "hal/mcu/am_hal_iom.h"
#include "hal/mcu/am_hal_ios.h"
#include "hal/mcu/am_hal_itm.h"
#include "hal/mcu/am_hal_mcu.h"
#include "hal/mcu/am_hal_mcuctrl.h"
#include "hal/mcu/am_hal_mcu_sysctrl.h"
#include "hal/mcu/am_hal_mpu.h"
#include "hal/mcu/am_hal_rram.h"
// #include "hal/mcu/am_hal_rram_recovery.h"
// #include "hal/mcu/am_hal_mspi.h"
#include "hal/mcu/am_hal_reset.h"
#include "hal/mcu/am_hal_rtc.h"
#include "hal/mcu/am_hal_sdhc.h"
#include "hal/mcu/am_hal_secure_ota.h"
#include "hal/mcu/am_hal_systick.h"
#include "hal/mcu/am_hal_tpiu.h"
#include "hal/mcu/am_hal_uart.h"
// #include "hal/mcu/am_hal_uart_stream.h"

//
// HAL common includes
//
#include "hal/am_hal_access.h"
// #include "hal/am_hal_adc.h"
// #include "hal/am_hal_audadc.h"
#include "hal/am_hal_dcu.h"
// #### INTERNAL BEGIN ####
// #include "hal/am_hal_gpdma.h"
// #### INTERNAL END ####
#include "hal/am_hal_gpio.h"
// #include "hal/am_hal_i2s.h"
#include "hal/am_hal_info.h"
// #include "hal/am_hal_infoc.h"
#include "hal/am_hal_mmu.h"
#include "hal/am_hal_pdm.h"
#include "hal/am_hal_pwrctrl.h"
#include "hal/am_hal_spotmgr.h"
#include "hal/am_hal_queue.h"
#include "hal/am_hal_security.h"
#include "hal/am_hal_stimer.h"
// #### INTERNAL BEGIN ####
//#include "hal/am_hal_shmem.h"
//#include "hal/am_hal_system.h"
// #### INTERNAL END ####
#include "hal/am_hal_timer.h"
#if defined(AM_PART_ATOMIQ110_USB_DUMMY_TYPE)
// TODO: (Will be remove) Use local USB_Type until CMSIS atomiq110 USB_Type is updated to latest.
#include "hal/am_hal_usb_atomiq110_dummy_type.h"
#endif
#include "hal/am_hal_usb.h"
// #include "hal/am_hal_usbcharger.h"
#include "hal/am_hal_utils.h"
// #include "hal/am_hal_vcomp.h"
// #include "hal/am_hal_wdt.h"
#include "hal/am_hal_gpu.h"

//
// INFO includes
//
#include "regs/am_mcu_atomiq110_rraminfo0.h"
#include "regs/am_mcu_atomiq110_rraminfo1.h"
#include "regs/am_mcu_atomiq110_otpinfo0.h"
#include "regs/am_mcu_atomiq110_otpinfo1.h"
#include "regs/am_mcu_atomiq110_otpinfoc.h"

#ifdef __cplusplus
}
#endif

#endif // AM_MCU_ATOMIQ_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
