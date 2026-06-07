//*****************************************************************************
//
//  am_util.h
//! @file
//!
//! @brief Top Include for the Apollo2 utility library.
//!
//! NSX-trimmed: the unified SDK's Apollo2 stack only needs the printf/string/
//! delay/debug utilities. The full AmbiqSuite am_util.h pulls in many modern
//! helpers (AES, BLE, DME, plot, ...) that reference HAL types not present in
//! the Apollo2-generation HAL, so they are intentionally omitted here.
//
//*****************************************************************************
#ifndef AM_UTIL_H
#define AM_UTIL_H

#include <stdint.h>
#include <stdbool.h>

#include "am_util_debug.h"
#include "am_util_delay.h"
#include "am_util_stdio.h"
#include "am_util_string.h"

#endif // AM_UTIL_H
