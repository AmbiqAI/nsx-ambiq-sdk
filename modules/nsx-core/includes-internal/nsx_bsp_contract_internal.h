#ifndef NSX_BSP_CONTRACT_INTERNAL_H
#define NSX_BSP_CONTRACT_INTERNAL_H

/* Defaults preserve the legacy SDK adapter. V2 values come only from the
 * validated BSP target, privately on nsx_system. */
#ifndef NSX_CORE_BSP_CONTRACT
#define NSX_CORE_BSP_CONTRACT 1
#endif
#if NSX_CORE_BSP_CONTRACT == 2
#if !defined(AM_PART_APOLLO330P) && !defined(AM_PART_APOLLO510)
#error "BSP contract v2 is not qualified for this core backend"
#endif
#if !defined(NSX_CORE_DEBUG_ITM) || !defined(NSX_CORE_DEBUG_UART)
#error "BSP contract v2 requires explicit debug capabilities"
#endif
#else
#if NSX_CORE_BSP_CONTRACT != 1
#error "Unsupported core BSP contract"
#endif
#define NSX_CORE_DEBUG_ITM 1
#define NSX_CORE_DEBUG_UART 1
#endif
#if (NSX_CORE_DEBUG_ITM != 0 && NSX_CORE_DEBUG_ITM != 1) || \
    (NSX_CORE_DEBUG_UART != 0 && NSX_CORE_DEBUG_UART != 1)
#error "Debug capabilities must be 0 or 1"
#endif
#endif
