/******************************************************************************
 *
 * sbrk.c - Bounded newlib heap-extension hook for nsx-core (GCC + ATfE).
 *
 * libnosys ships a stub _sbrk() that grows the heap monotonically with no
 * bounds check, which silently corrupts whatever happens to live above the
 * heap region.  Historically this hit nsx-core hard because the .heap section
 * was placed BEFORE .data/.bss in TCM, so libc malloc would walk straight
 * through .data once it exhausted the small fixed g_pui32Heap[] buffer.
 *
 * The linker scripts have been reordered so .heap occupies all remaining TCM
 * between .bss and the top of the region; this _sbrk() honors the
 * __HeapBase / __HeapLimit symbols the linker exports and returns
 * (void *)-1 with errno=ENOMEM on overflow instead of clobbering memory.
 *
 * IMPORTANT (link-order trap): this strong definition alone is NOT enough.
 * The prebuilt AmbiqSuite libam_hal.a also defines a WEAK `_sbrk` stub
 * (am_hal_global.o's GCC link-warning silencers) that returns the constant
 * ENOSYS (0x58) as if it were a heap-break pointer. Because static-archive
 * extraction is demand-driven and `_sbrk` is typically first demanded very
 * late in the link (newlib malloc -> _sbrk_r, e.g. from libstdc++'s
 * pre-main exception-pool ctor), the weak stub can win and this file's
 * object is never scanned — malloc then writes through 0x58 (silent ITCM
 * corruption on apollo510-family, HardFault before main() on apollo330P).
 * nsx-core's CMakeLists.txt therefore force-links this object with
 * `-Wl,-u,_sbrk` alongside the analogous `-u,__wrap__*` flags for
 * nsx_retarget.c. Do not remove either half without the other.
 *
 * armclang uses ARM_LIB_HEAP / ARM_LIB_STACK + the Arm runtime, so this file
 * is only compiled into GCC and ATfE builds (see CMakeLists.txt).
 *
 *****************************************************************************/

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

extern uint32_t __HeapBase;
extern uint32_t __HeapLimit;

void *_sbrk(ptrdiff_t incr) {
    static char *heap_end = (char *)0;

    if (heap_end == (char *)0) {
        heap_end = (char *)&__HeapBase;
    }

    char *const limit = (char *)&__HeapLimit;
    char *const prev  = heap_end;

    /* Reject negative shrink that would underflow below the base, and any
     * request that would push the new break past __HeapLimit. */
    if (incr < 0 && (prev + incr) < (char *)&__HeapBase) {
        errno = ENOMEM;
        return (void *)-1;
    }
    if (incr > 0 && (prev > limit - incr)) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end = prev + incr;
    return (void *)prev;
}
