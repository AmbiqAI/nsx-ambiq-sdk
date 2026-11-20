#pragma once

#include <stdbool.h>
#include <stdint.h>
#if defined(AM_PART_APOLLO4L) || defined(AM_PART_APOLLO4P)
#define AM_PART_APOLLO4_API
#else
#define AM_PART_APOLLO5_API
#endif
#include TEST_DEVICE
#include "hal/am_hal_status.h"
#include "hal/am_hal_global.h"
#include "hal/mcu/am_hal_rtc.h"
#include "hal/am_hal_timer.h"
#include "hal/am_hal_stimer.h"
