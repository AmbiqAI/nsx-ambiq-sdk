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
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_HAL_GPU_H
#define AM_HAL_GPU_H

#include "am_mcu_apollo.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
//
//! CMSIS-style macro for handling a variable GPU module number.
//
//*****************************************************************************
#define GPUn(n) ((GPU_Type*)(GPU_BASE + (n * (GPU_BASE - GPU_BASE))))

//*****************************************************************************
//! @brief GPU interrupt service routine callback function pointer type
//!
//! Upper layer GPU stack uses it to define a callback function to receive the
//! the GPU interrupt status and registers it by 'am_hal_gpu_interrupt_register_callback'
//! function.
//
//*****************************************************************************
typedef void (*am_hal_gpu_isr_callback_t)(void *pContext, uint32_t ui32IrqStatus);

//*****************************************************************************
//
// Public Functions
//
//*****************************************************************************
//*****************************************************************************
//! @brief Initialize the GPU Driver and Hardware.
//!
//! @param ui32Module - GPU module number.
//! @param ppHandle - Returned GPU HAL handle.
//!
//! @return AM_HAL_STATUS_SUCCESS or applicable error status.
//*****************************************************************************
uint32_t am_hal_gpu_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//! @brief Deinitialize the GPU Driver.
//!
//! @param pHandle - GPU HAL handle.
//!
//! @return AM_HAL_STATUS_SUCCESS or applicable error status.
//*****************************************************************************
uint32_t am_hal_gpu_deinitialize(void *pHandle);

//*****************************************************************************
//! @brief Read a GPU register by D/AVE NX word register offset.
//!
//! @param pHandle - GPU HAL handle.
//! @param ui32Register - D/AVE NX word register offset.
//!
//! @return Register value.
//*****************************************************************************
uint32_t am_hal_gpu_reg_read(void *pHandle, uint32_t ui32Register);

//*****************************************************************************
//! @brief Write a GPU register by D/AVE NX word register offset.
//!
//! @param pHandle - GPU HAL handle.
//! @param ui32Register - D/AVE NX word register offset.
//! @param ui32Value - Register value to write.
//!
//! @return None.
//*****************************************************************************
void am_hal_gpu_reg_write(void *pHandle, uint32_t ui32Register, uint32_t ui32Value);

// Interrupt Control
//*****************************************************************************
//
//! @brief GPU enable interrupts function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function enables the specific indicated interrupts.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_enable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief GPU disable interrupts function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function disables the specified interrupts.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_disable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief GPU get interrupt status.
//!
//! @param pHandle       - handle for the interface.
//! @param pui32Status   - pointer to a uint32_t to return the interrupt status.
//! @param bEnabledOnly  - if true, return only enabled interrupt bits.
//!
//! @return status       - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_status(void *pHandle, uint32_t *pui32Status, bool bEnabledOnly);

//*****************************************************************************
//
//! @brief GPU interrupt clear.
//!
//! @param pHandle     - handle for the interface.
//! @param ui32IntMask - interrupt status bits to clear (W1C).
//!
//! @return status     - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_clear(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief GPU interrupt service routine.
//!
//! @param pHandle       - handle for the interface.
//! @param ui32IntStatus - interrupt status flags from DNXREGCONTROLIRQSTATE0.
//!
//! @return status       - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_service(void *pHandle, uint32_t ui32IntStatus);

//*****************************************************************************
//
//! @brief GPU interrupt register callback function.
//!
//! @param pHandle     - handle for the interface.
//! @param pfnCallback - pointer to the callback function, or NULL to unregister.
//! @param pContext    - pointer to the context.
//!
//! @return status     - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_interrupt_register_callback(void *pHandle,
                                                        am_hal_gpu_isr_callback_t pfnCallback,
                                                        void *pContext);

//*****************************************************************************
//
//! @brief GPU wait until finished function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32TimeoutUs - timeout in microseconds.
//!
//! This function waits until the GPU is finished.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_gpu_wait_until_finished(void *pHandle, uint32_t ui32TimeoutUs);

//*****************************************************************************
//
//! @brief GPU power control function
//!
//! @param pHandle      - handle for the interface.
//! @param ePowerState  - power state.
//! @param bRetainState - retain state.
//*****************************************************************************
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
