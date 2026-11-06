//*****************************************************************************
//
//! @file am_mcu_apollo.h
//!
//! @brief Top Include for Apollo4 Lite class devices.
//!
//! This file provides all the includes necessary for an apollo device.
//!
//! @addtogroup hal Hardware Abstraction Layer (HAL)
//
//! @defgroup apollo4l_hal apollo4l
//! @ingroup hal
//! @{
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

#ifndef AM_MCU_APOLLO_H
#define AM_MCU_APOLLO_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// AM_PART_APOLLO4_API indicates that this device uses the Apollo4 API.
//
//*****************************************************************************
#define AM_PART_APOLLO4_API     1

//*****************************************************************************
//
// Define AM_CMSIS_REGS to indicate that CMSIS registers are supported.
//
//*****************************************************************************
#define AM_CMSIS_REGS           1

//*****************************************************************************
//
// Apollo family device type.
//
//*****************************************************************************
#define IS_RUN_ON_APOLLO4L()    (((MCUCTRL->CHIPPN & MCUCTRL_CHIPPN_PN_Msk) >> MCUCTRL_CHIPPN_PN_Pos) ==  MCUCTRL_CHIPPN_PN_APOLLO4L)

//*****************************************************************************
//
// C99
//
//*****************************************************************************
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(__XTENSA__)
#define __XTENSA__ 0
#endif

#if __XTENSA__
#include "core_ambiq_xtensa.h"
#include "apollo4_generic.h"
#else
#include "apollo4l.h"
#endif

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

#if __XTENSA__
#include "regs/am_reg_dsp.h"
#else
#include "regs/am_reg_mcu.h"
#endif

//*****************************************************************************
//
// HAL
//
//*****************************************************************************
#include "hal/am_hal_global.h"
#include "hal/am_hal_status.h"

#if __XTENSA__
#include "hal/dsp/am_hal_dsp.h"
#else

//
// MCU includes
//
#include "hal/mcu/am_hal_mram.h"
#include "hal/mcu/am_hal_cmdq.h"
#include "hal/mcu/am_hal_cachectrl.h"
#include "hal/mcu/am_hal_fault.h"
#include "hal/mcu/am_hal_itm.h"
#include "hal/am_hal_sysctrl.h"
#include "hal/mcu/am_hal_iom.h"
#include "hal/mcu/am_hal_ios.h"
#include "hal/mcu/am_hal_mcu.h"
#include "hal/mcu/am_hal_mcuctrl.h"
#include "hal/mcu/am_hal_mspi.h"
#include "hal/mcu/am_hal_reset.h"
#include "hal/mcu/am_hal_mcu_sysctrl.h"
#include "hal/mcu/am_hal_systick.h"
#include "hal/mcu/am_hal_tpiu.h"
#include "hal/mcu/am_hal_uart.h"
#include "hal/mcu/am_hal_clkgen.h"
#include "hal/mcu/am_hal_rtc.h"
#include "hal/mcu/am_hal_secure_ota.h"
#include "hal/mcu/am_hal_card_host.h"
#include "hal/mcu/am_hal_card.h"
#include "hal/mcu/am_hal_sdhc.h"
#include "hal/mcu/am_hal_mpu.h"
#endif

//
// HAL common includes
//
#include "hal/am_hal_access.h"
#include "hal/am_hal_adc.h"
#include "hal/am_hal_i2s.h"
#include "hal/am_hal_pin.h"
#include "hal/am_hal_gpio.h"
#include "hal/am_hal_pdm.h"
#include "hal/am_hal_pwrctrl.h"
#include "hal/am_hal_queue.h"
#include "hal/am_hal_stimer.h"
#include "hal/am_hal_system.h"
#include "hal/am_hal_timer.h"
#include "hal/am_hal_utils.h"
#include "hal/am_hal_security.h"
#include "hal/am_hal_wdt.h"
#include "hal/am_hal_otp.h"
#include "hal/am_hal_dcu.h"
// #### INTERNAL BEGIN ####
// TODO: remove internal once VCOMP is ready for public release
#include "hal/am_hal_vcomp.h"
// #### INTERNAL END ####
#include "regs/am_mcu_apollo4l_info0.h"
#include "regs/am_mcu_apollo4l_info1.h"
#include "regs/am_mcu_apollo4l_otp.h"

#ifdef NEMA_PLATFORM
#include "nema_sys_defs.h"
#endif

#ifdef __cplusplus
}
#endif

#endif // AM_MCU_APOLLO_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
