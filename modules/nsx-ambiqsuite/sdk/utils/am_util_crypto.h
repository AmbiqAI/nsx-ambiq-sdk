//*****************************************************************************
//
//! @file am_util_crypto.h
//!
//! @brief Cryptographic Utility Functions
//!
//! @addtogroup crypto_utils Crypto Utility Functions
//! @ingroup utils
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

#ifndef AM_UTIL_CRYPTO_H
#define AM_UTIL_CRYPTO_H

#include <time.h>

/* As required by POSIX.1-2008, declare tm as incomplete type.
   The actual definition is in time.h.
   In C++, <time.h> brings std::tm into the global namespace via a
   using-declaration, so this redundant global `struct tm;` is both
   unnecessary and ill-formed (it clashes with the using-declared name under
   Arm Compiler 6 / LLVM ATfE libc). Restrict it to C translation units. */
#ifndef __cplusplus
struct tm;
#endif

#if !defined(AM_PART_APOLLO4_API) && !defined(AM_PART_APOLLO5_API)
#error "Error: am_util_crypto.h only supports Apollo4 and Apollo5."
#endif

//
// Memory Pools
//
#define AM_UTIL_CRYPTO_VARIABLE_POOL_SIZE      512
#define AM_UTIL_CRYPTO_FIXED_BUFFER_SIZE       20
#define AM_UTIL_CRYPTO_FIXED_BUFFER_COUNT      1024

//
// LibSWD configuration
//
#define AM_UTIL_CRYPTO_WRITE_BLOCK_SIZE        128
#define AM_UTIL_CRYPTO_READ_BLOCK_SIZE         32

//*****************************************************************************
//
//! @brief Malloc implemtation tailored to LibCrypto's needs.
//!
//! @param size - size of buffers to malloc
//!
//! @return pointer to space malloc'ed
//
//*****************************************************************************
extern void * am_util_crypto_malloc(size_t size);

//*****************************************************************************
//
//! @brief Calloc implementation tailored to LibCrypto's needs.
//!
//! @param num - number of buffers to calloc
//! @param size - size of buffers to calloc
//!
//! @return pointer to space calloc'ed
//
//*****************************************************************************
extern void * am_util_crypto_calloc(size_t num, size_t size);

//*****************************************************************************
//
//! @brief Minimal implementation of Crypto Free.
//
//! @note This implementation of free can only free memory from the fixed-size
//! buffer pool. It ignores all other requests.
//!
//! @param ptr - pointer to buffer to free
//
//*****************************************************************************
extern void am_util_crypto_free(void *ptr);

//*****************************************************************************
//
//! @brief Statistics function for malloc
//
//*****************************************************************************
extern void am_util_crypto_print_malloc_stats(void);

//*****************************************************************************
//
//! @brief Crypto Exit
//!
//! @param exitCode - Exit Code
//
//*****************************************************************************
extern void am_util_crypto_exit(uint32_t exitCode);

//*****************************************************************************
//
//! @brief Crypto Abort
//
//*****************************************************************************
extern void am_util_crypto_abort(void);

//*****************************************************************************
//
//! @brief Crypto Get Time for mbedtls libraries
//!
//! @note To be used with cryptocell library builds only. RTC structure should
//!       be defined in code per crypto_self_test example
//!
//! @param time - pointer to time structure - will be unsued and is a carryover
//!               from the library
//!
//! @return time_t - Calculated time in seconds since 1/1/1970 until current
//!                  RTC time
//
//*****************************************************************************
extern time_t am_util_crypto_get_time( time_t * time );

//*****************************************************************************
//
//! @brief Crypto Set Ambiq RTC structure
//!
//! @note To be used with cryptocell library builds only
//!
//! Use this function to create a local structure in user code to pass for
//! use in am_util_crypto_get_time
//
//*****************************************************************************
extern void am_util_crypto_ambiq_time_structure_set(am_hal_rtc_time_t *atm);

//*****************************************************************************
//
//! @brief Crypto Set gmtime structure
//!
//! @note To be used with cryptocell library builds only
//!
//! Use this function to create a local structure in user code to pass for
//! use in am_util_crypto_get_time
//
//*****************************************************************************
extern void am_util_crypto_gm_time_structure_set(struct tm *tm);

#endif // AM_UTIL_CRYPTO_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

