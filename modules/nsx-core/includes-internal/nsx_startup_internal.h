#ifndef NSX_STARTUP_INTERNAL_H
#define NSX_STARTUP_INTERNAL_H

/* Shared by NSX system/power implementations; not an application API. */
#include <stdint.h>

#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P) || \
    defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L) || \
    defined(AM_PART_APOLLO510) || defined(AM_PART_APOLLO510B) || \
    defined(AM_PART_APOLLO5A) || defined(AM_PART_APOLLO5B) || \
    defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
#define NSX_STARTUP_GUARDED 1
uint32_t nsx_startup_claim(void);
uint32_t nsx_startup_finish(uint32_t status);
#else
#define NSX_STARTUP_GUARDED 0
static inline uint32_t nsx_startup_claim(void) { return 0; }
static inline uint32_t nsx_startup_finish(uint32_t status) { return status; }
#endif

#endif
