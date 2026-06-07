//*****************************************************************************
//
//! @file am_hal_puf.h
//!
//! @brief Hardware abstraction for the PUF (Physically Unclonable Function)
//!
//! @addtogroup puf_ap510L PUF - Physically Unclonable Function
//! @ingroup apollo330P_hal
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

#ifndef AM_HAL_PUF_H
#define AM_HAL_PUF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Public PUF entropy APIs
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Read PUF UID data into a buffer.
//!
//! This function retrieves one or more 32-bit words from the PUF UID registers
//! (UID_PUF_000 through UID_PUF_031) and copies them into the caller's buffer.
//!
//! @param pui32Buffer Pointer to a uint32_t array to receive the UID words.
//! @param ui32StartWord Starting word index (0..31).
//! @param ui32NumWords Number of consecutive words to read (1..32).
//!
//! @return AM_HAL_STATUS_SUCCESS on success, AM_HAL_STATUS_FAIL on failure.
//
//*****************************************************************************
extern uint32_t am_hal_puf_get_uid(uint32_t *pui32Buffer, uint32_t ui32StartWord, uint32_t ui32NumWords);

//*****************************************************************************
//
//! @brief Retrieve entropy into a buffer.
//!
//! @param buffer Pointer to output buffer to receive entropy bytes.
//! @param length Number of bytes requested to fill in buffer.
//!
//! @return AM_HAL_STATUS_SUCCESS on success, AM_HAL_STATUS_FAIL on failure.
//
//*****************************************************************************
extern uint32_t am_hal_puf_get_entropy(uint8_t *buffer, uint16_t length);

//*****************************************************************************
//
//! @brief Initialize the entropy peripheral (power on OTP).
//!
//! @return AM_HAL_STATUS_SUCCESS on success, AM_HAL_STATUS_FAIL on failure.
//
//*****************************************************************************
extern uint32_t am_hal_puf_entropy_init(void);

//*****************************************************************************
//
//! @brief Deinitialize the entropy peripheral (power down OTP).
//!
//! @return AM_HAL_STATUS_SUCCESS on success, AM_HAL_STATUS_FAIL on failure.
//
//*****************************************************************************
extern uint32_t am_hal_puf_entropy_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_PUF_H


//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
