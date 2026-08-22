//*****************************************************************************
//
//! @file am_hal_mmu.h
//!
//! @brief Memory Management Unit functions that support virtual memory
//!        addressing.
//!
//! @addtogroup mm_at110 MMU - Memory Management Unit
//! @ingroup atomiq110_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_MMU_H
#define AM_HAL_MMU_H

#ifdef __cplusplus
extern "C"
{
#endif


//*****************************************************************************
//
//! @Defines
//
//*****************************************************************************
#define MMUMAXPIDS                  16  // Maximum number of PIDs [3:0]
#define MMUPIDVALIDENTRY            0xFFFFFFFF

//
// Specify the maximum number of TLB entries.
// For Atomiq110, this value is 32.
// This value must match the value reported by the register MAXTLB + 1.
//
#define AM_HAL_MMU_MAXTLBENTRIES    32

//*****************************************************************************
// Virtual address format
//  Upper PREFIX:           [31:27]     Always 00001b
//  Virtual Page Number     [26:12]     Index into TLB, aka TLB TAG
//  Page offset             [11:0]      4KB page
//*****************************************************************************
//
// Virtual Address defines
//
#define AM_HAL_MMU_VAPREFIX_Pos     27
#define AM_HAL_MMU_VAPREFIX_Msk     (0x1F << AM_HAL_MMU_VAPREFIX_Pos)
#define AM_HAL_MMU_VAVPN_Pos        12
#define AM_HAL_MMU_VAVPN_Msk        (0x7FFF << AM_HAL_MMU_VAVPN_Pos)
#define AM_HAL_MMU_VAPGOFF_Pos      0
#define AM_HAL_MMU_VAPGOFF_Msk      (0xFFF << AM_HAL_MMU_VAPGOFF_Pos)
#define AM_HAL_MMU_VAPREFIX         (0x01 << AM_HAL_MMU_VAPREFIX_Pos)   // VA bits 31:27 (0x08xxxxxx)

#define AM_HAL_MMU_PA_PAGE_SIZE     4096
#define AM_HAL_MMU_PHYS_PAGE_SIZE   4096

//
// Virtual address range
//
#define AM_HAL_MMU_VA_ADDR          0x08000000
#define AM_HAL_MMU_VA_END_ADDR      (AM_HAL_MMU_VA_ADDR + (128 * 1024 * 1024) - 1)


//*****************************************************************************
//
//! @brief MMU configuration structure.
//
//*****************************************************************************
typedef struct
{
    //
    //! Virtual address range (specified as 32-bit addresses)
    //
    uint32_t ui32VA_start;
    uint32_t ui32VA_end;

    //
    //! Page table base address and size (specified as 32-bit address)
    //
    uint32_t ui32pgtbl_baseaddr;
    uint32_t ui32pgtbl_size;

    //
    //! Misc config
    //
    bool bPTrefcntupdate;
    bool bPTprivaccess;

} am_hal_mmu_config_t;


//
//! MMU control request for am_hal_mmu_control()
//
typedef enum
{
    //! Return the last fault
    AM_HAL_MMU_LAST_FAULT = 0,

    //! Invalidate all TLB entries
    AM_HAL_MMU_TLB_ALL_INVALIDATE,

    //! Invalidate all TLB entries associated with the given PID
    AM_HAL_MMU_TLB_PID_INVALIDATE,

    //! Invalidate all TLB entries matching the given PID and virtual page
    AM_HAL_MMU_TLB_ENTRY_INVALIDATE,

    //! Set PTBASE and PTSIZE
    AM_HAL_MMU_SET_PTBASE,

    //! Reset all TLB hit counts
    AM_HAL_MMU_HIT_COUNT_RESET,

} am_hal_mmu_request_e;


//
// Enumerations for eCacheClean of am_hal_mmu_context_set().
//
typedef enum
{
    //! No clean/invalidate
    AM_HAL_MMU_CONTEXT_CLEAN_NONE = 0,

    //! Dcache clean
    AM_HAL_MMU_CONTEXT_CLEAN_DCACHE,

    //! Dcache clean and invalidate
    AM_HAL_MMU_CONTEXT_CLEAN_INVALIDATE_DCACHE,

    //! Icache clean
    AM_HAL_MMU_CONTEXT_CLEAN_ICACHE,

    //! Icache clean and invalidate
    AM_HAL_MMU_CONTEXT_CLEAN_INVALIDATE_ICACHE,

    //! Dcache and Icache clean
    AM_HAL_MMU_CONTEXT_CLEAN_DICACHE,

    //! Dcache and Icache clean and invalidate
    AM_HAL_MMU_CONTEXT_CLEAN_INVALIDATE_DICACHE,
} am_hal_mmu_context_clean_e;


//
//! MMU performance structure
//
typedef struct
{
    //
    // Performance counter
    // Total TLB accesses since last reset.
    //
    uint32_t ui32TLBaccesses;

    //
    // Performance counter
    // TLB hits since last reset.
    //
    uint32_t ui32TLBhits;

    //
    // Performance counter
    // Total successful Page Table lookups since last reset.
    // Same as TLB misses.
    //
    uint32_t ui32PTlookups;

    //
    // Performance counter
    // Total Page Table failures (decode errors returned) since last reset.
    //
    uint32_t ui32PThits;

    //
    // Performance counter
    // Total Page Table failures (decode errors returned) since last reset.
    //
    uint32_t ui32PTmisses;

} am_hal_mmu_perfor_t;

//*****************************************************************************
//!
//! Structure for a TLB entry
//!
//*****************************************************************************
typedef struct
{
    volatile uint32_t   physpage:   20;     // [19:0]
    volatile uint32_t   hitcount:    8;     // [27:20]
    volatile uint32_t   PID:         4;     // [31:28]
    volatile uint32_t   valid:       1;     // [32:32]
    volatile uint32_t   ref:         1;     // [33:33]
    volatile uint32_t   tag:        15;     // [48:34]
    volatile uint32_t   rsvd:       15;     // Pad out to the next full word
} am_hal_mmu_tlb_t;


//*****************************************************************************
//!
//! Structure for a page table entry
//!
//*****************************************************************************
typedef union
{
    uint32_t    u32;
    struct
    {
        volatile uint32_t   refcnt:      8;     // [7:0]
        volatile uint32_t   rsvd:        3;     // [10:8]
        volatile uint32_t   valid:       1;     // [11:11]
        volatile uint32_t   physpage:   20;     // [31:12]
    } s;
} am_hal_mmu_pte_t;

#define  AM_HAL_MMU_PTE_PHYSPAGE_Msk    0xFFFFF000
#define  AM_HAL_MMU_PTE_PHYSPAGE_Pos    12
#define  AM_HAL_MMU_PTE_VALID_Msk       0x00000800
#define  AM_HAL_MMU_PTE_VALID_Pos       11
#define  AM_HAL_MMU_PTE_RSVD_Msk        0x00000700
#define  AM_HAL_MMU_PTE_RSVD_Pos        8
#define  AM_HAL_MMU_PTE_REFCNT_Msk      0x000000FF
#define  AM_HAL_MMU_PTE_REFCNT_Pos      0


//*****************************************************************************
//
// External functions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief MMU initialization.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_initialize(void **ppHandle);

//*****************************************************************************
//
//! @brief MMU power control.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_powercontrol(void *pHandle,
                                        am_hal_sysctrl_power_state_e ePowerState,
                                        bool bRetainState);

//*****************************************************************************
//
//! @brief Performs required deinitialization tasks
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_deinit(void *pHandle);

//*****************************************************************************
//
//! @brief MMU control function
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_control(void *pHandle, am_hal_mmu_request_e eControlRequest, void *pArgs);

//*****************************************************************************
//
//! @brief MMU configuration function
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_config(void *pHandle, const am_hal_mmu_config_t *psConfig);

//*****************************************************************************
//
//! @brief Returns the current MMU configuration.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_config_get(void *pHandle, am_hal_mmu_config_t *psConfig);

//*****************************************************************************
//
//! @brief Enable the MMU
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_enable(void *pHandle);

//*****************************************************************************
//
//! @brief Disable the MMU
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_disable(void *pHandle);

//*****************************************************************************
//
//! @brief MMU context set function
//!
// TODO Complete the following descriptions:
//! @param pHandle         - Handle as returned from am_hal_mmu_initialize()
//! @param ui32PID         - The PID number, 0 - 15.
//! @param ui32VAoffset    - The VA Offset value.
//! @param bCacheClean     - Do a cache clean operation (typically set to true).
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_context_set(void *pHandle,
                                       uint32_t ui32PID,
                                       uint32_t ui32VAoffset,
                                       am_hal_mmu_context_clean_e eCacheClean);

//*****************************************************************************
//
//! @brief MMU context get function, returns the current context.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_context_get(void *pHandle, uint32_t *pui32PID, uint32_t *pui32VAoffset);

//*****************************************************************************
//
//! @brief Return most recent performance parameters
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_performance_get(void *pHandle, am_hal_mmu_perfor_t *psPerformance);

//*****************************************************************************
//
//! @brief Return current TLB
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_TLB_get(void *pHandle,
                                   am_hal_mmu_tlb_t sTLBentry[AM_HAL_MMU_MAXTLBENTRIES]);

//*****************************************************************************
//
//! @brief Set up a page table in memory.
//!
//! @param pHandle          - Handle as returned from am_hal_mmu_initialize()
//! @param pui32PTbase      - Provide the base physicial address of the page table.
//! @param ui32PTnumentries - Description here
//! @param ui32PhysicalPage - The physical address of the first indexed page in
//!                           the page table. Note that this parameter is a
//!                           page-aligned, 32-bit address.
//! @param bValid           - True if the page table entries are to be
//!                           initialized as valid.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_PT_setup(void *pHandle,
                                    uint32_t *pui32PTbase,
                                    uint32_t ui32PTnumentries,
                                    uint32_t ui32PhysicalPage,
                                    bool bValid);

//*****************************************************************************
//
//! @brief Return a page table
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_PT_get(void *pHandle, uint32_t *pui32PT);

//*****************************************************************************
//
//! @brief Compute the Virtual Address Offset value
//!
// TODO Complete the following descriptions:
//! @param ui32VirtualAddr  - The virtual address to be directed to a page table
//!                           index.
//! @param ui32PToffset     - The page table offset, in bytes but word aligned,
//!                           that the given VA will reference.
//! @param pui32VAoffset    - Pointer to the variable to receive the computed value.
//!
//!
//! @return Standard HAL status code.
//
//*****************************************************************************
extern uint32_t am_hal_mmu_vaoffset_compute(uint32_t ui32VirtualAddr,
                                            uint32_t ui32PToffset,
                                            uint32_t *pui32VAoffset);


#ifdef __cplusplus
}
#endif

#endif // AM_HAL_MMU_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
