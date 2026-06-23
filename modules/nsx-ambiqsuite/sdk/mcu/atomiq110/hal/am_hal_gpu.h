//*****************************************************************************
//
//  am_hal_gpu.h
//! @file
//!
//! @brief Hardware Abstraction Layer for GPU.
//!
//! @addtogroup gpu GPU
//! @ingroup atomiq110_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_HAL_GPU_H
#define AM_HAL_GPU_H

#include "am_mcu_apollo.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
  #endif

typedef void (*am_hal_gpu_isr_callback_t)(void *pContext, uint32_t ui32IrqStatus);
//*****************************************************************************
//
// Driver Status Codes
//
//*****************************************************************************
typedef enum
{
    AM_HAL_GPU_SUCCESS = 0,
    AM_HAL_GPU_ERROR_GENERIC = 1,
    AM_HAL_GPU_ERROR_INIT_FAILED = 2,
    AM_HAL_GPU_ERROR_TIMEOUT = 3,
} am_hal_gpu_status_e;

//*****************************************************************************
//
// Public Functions
//
//*****************************************************************************
// Initialize the GPU Driver and Hardware
uint32_t am_hal_gpu_initialize(uint32_t ui32Module, void **ppHandle);

// Deinitialize the GPU Driver
uint32_t am_hal_gpu_deinitialize(void *pHandle);

// Register Accessors
uint32_t am_hal_gpu_reg_read(void *pHandle, uint32_t ui32Register);
void am_hal_gpu_reg_write(void *pHandle, uint32_t ui32Register, uint32_t ui32Value);

// Interrupt Control
extern uint32_t am_hal_gpu_interrupt_enable(void *pHandle, uint32_t ui32IntMask);
extern uint32_t am_hal_gpu_interrupt_disable(void *pHandle, uint32_t ui32IntMask);
extern uint32_t am_hal_gpu_interrupt_status(void *pHandle, uint32_t *pui32Status, bool bEnabledOnly);
extern uint32_t am_hal_gpu_interrupt_clear(void *pHandle, uint32_t ui32IntMask);
extern uint32_t am_hal_gpu_interrupt_service(void *pHandle, uint32_t ui32IntStatus);
extern uint32_t am_hal_gpu_interrupt_register_callback(am_hal_gpu_isr_callback_t pfnCallback, void *pContext);
// Power control
uint32_t am_hal_gpu_power_control(void *pHandle, uint32_t ePowerState, bool bRetainState);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_GPU_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
