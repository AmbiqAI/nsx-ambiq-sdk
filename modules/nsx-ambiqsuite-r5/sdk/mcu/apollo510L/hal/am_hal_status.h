//*****************************************************************************
//
//! @file am_hal_status.h
//!
//! @brief Global status return codes for HAL interface.
//!
//! @addtogroup status3_ap510L Status - Global Status Return Codes
//! @ingroup apollo510L_hal
//! @{
//!
//! Purpose: This module defines a comprehensive set of status return codes
//!          used throughout the Apollo5 HAL interface. It provides standardized
//!          error reporting and status indication across all HAL functions,
//!          enabling consistent error handling and status tracking in
//!          Apollo5-based applications.
//!
//! @section hal_status_features Key Features
//!
//! 1. @b Standardized @b Codes: Common return values across all HAL modules
//! 2. @b Error @b Categorization: Distinct codes for different error types
//! 3. @b Module @b Extensions: Support for module-specific error codes
//! 4. @b Status @b Tracking: Comprehensive operation result reporting
//! 5. @b Cross-Module @b Compatibility: Unified status handling system
//!
//! @section hal_status_functionality Functionality
//!
//! - Success/failure status indication
//! - Resource usage state tracking
//! - Invalid parameter detection
//! - Hardware error reporting
//! - Memory operation status
//! - Timeout condition indication
//!
//! @section hal_status_usage Usage
//!
//! 1. Include am_hal_status.h in HAL implementation files
//! 2. Return appropriate status codes from HAL functions
//! 3. Check return values for error conditions
//! 4. Handle module-specific error codes when needed
//! 5. Use for consistent error reporting across modules
//!
//! @section hal_status_codes Status Codes
//!
//! - AM_HAL_STATUS_SUCCESS: Operation completed successfully
//! - AM_HAL_STATUS_FAIL: General failure
//! - AM_HAL_STATUS_INVALID_HANDLE: Invalid handle/instance
//! - AM_HAL_STATUS_IN_USE: Resource already in use
//! - AM_HAL_STATUS_TIMEOUT: Operation timed out
//! - AM_HAL_STATUS_OUT_OF_RANGE: Parameter out of valid range
//! - AM_HAL_STATUS_INVALID_ARG: Invalid argument provided
//! - AM_HAL_STATUS_INVALID_OPERATION: Operation not allowed
//! - AM_HAL_STATUS_MEM_ERR: Memory operation error
//! - AM_HAL_STATUS_HW_ERR: Hardware error occurred
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_STATUS_H
#define AM_HAL_STATUS_H

#ifdef __cplusplus
extern "C"
{
#endif

//
//! Global Status Returns
//
typedef enum
{
AM_HAL_STATUS_SUCCESS,
AM_HAL_STATUS_FAIL,
AM_HAL_STATUS_INVALID_HANDLE,
AM_HAL_STATUS_IN_USE,
AM_HAL_STATUS_TIMEOUT,
AM_HAL_STATUS_OUT_OF_RANGE,
AM_HAL_STATUS_INVALID_ARG,
AM_HAL_STATUS_INVALID_OPERATION,
AM_HAL_STATUS_MEM_ERR,
AM_HAL_STATUS_HW_ERR,
AM_HAL_STATUS_MODULE_SPECIFIC_START = 0x08000000,
} am_hal_status_e;

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_STATUS_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

