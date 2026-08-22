//*****************************************************************************
//
//! @file am_hal_bootrom_helper.h
//!
//! @brief BootROM Helper Function Table
//!
//! @addtogroup bootrom_at110 Bootrom Functionality
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
#ifndef AM_HAL_BOOTROM_HELPER_H
#define AM_HAL_BOOTROM_HELPER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

//*****************************************************************************
//
//  BootROM Helper Function calling documentation.
//
// ****************************************************************************

//*****************************************************************************
//
//! Structure of pointers to helper functions invoking flash operations.
//
//! The functions we are pointing to here are in the Atomiq110
//! integrated BOOTROM.
//
//*****************************************************************************
typedef struct am_hal_bootrom_helper_struct
{
    //
    //! MRAM Infospace programming function.
    //
    int  (*nv_program_info_area)(uint32_t, uint32_t *, uint32_t, uint32_t);

    //
    //! Helpful utilities.
    //
    int  (*nv_program_main2)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

    uint32_t (*bootrom_util_read_word)( uint32_t *);
    void (*bootrom_util_write_word)( uint32_t *, uint32_t);

    //
    //! Valid address range.
    //
    bool (*valid_address_range)(void *pStart, uint32_t size, uint32_t regionMask);
    //
    //! Cycle accurate delay function.
    //
    void (*bootrom_delay_cycles)(uint32_t ui32Cycles);

    //
    //! OTP Infospace programming function.
    //
    int  (*otp_program_info_area)(uint32_t, uint32_t*, uint32_t, uint32_t);

    //
    //! BootROM version info.
    //
    uint32_t (*bootrom_version_info)(void);

} am_hal_bootrom_helper_t;

//*****************************************************************************
//
//  Global data
//
//*****************************************************************************
extern const am_hal_bootrom_helper_t g_am_hal_bootrom_helper;


/******************************************************************************
 *
 * @brief entry point for programming the INFO0(customer) block NVRAM
 *
 * Calling this function programs multiple words in the INFO0 block on one nv instance
 *
 * @param value         customer info key
 * @param pSrc          pointer to word aligned array of data to program into the NV INFO block
 * @param Offset        word offset in to info block (0x01 means the second 32 bit word)
 *                      NOTE Offset == 0 --> first word in INFO0.
 * @param NumberOfWords number of words to program
 *
 * @return       0 for success, non-zero for failure
 *               1 Invalid Key
 *               2 Invalid destination start address
 *               3 Invalid destination end address
 *               4 Invalid source buffer start address
 *               5 Invalid source buffer end address
 *               6 Hardware error
 *               7 Reserved
 *               8 Hardware error
 *               9 OTP interrupts detected
 *              10 Memory not powered on
 *              11 Area is write protected
 *              12 Area is copy protected
 *              13 Internal error
 *
 *****************************************************************************/
int
nv_program_info_area(uint32_t  value,
                     uint32_t *pSrc,
                     uint32_t  Offset,
                     uint32_t  NumberOfWords);

/******************************************************************************
 *
 * @brief Enhanced version of nv_program_main which passes through more of
 * the features of nv_program.
 *
 * Calling this function programs up to N words of the MAIN block of NVRAM
 *
 * @param value         customer program key
 * @param Program_nWipe boolean option: 1 = Addr_WipeData is the source address (pointer)
 *                                          (pSrc can not point back in to NVRAM itself)
 *                                      0 = Addr_WipeData is data word to write repeatedly during wipe
 * @param Addr_WipeData if Program_nWipe is 0, then single word value to write repeatedly to NVRAM
 *                      if Program_nWipe is 1, then 32-bit start byte address of region to write to NVRAM
 * @param WordOffset    word offset from the start of NVRAM to program
 * @param NumberOfWords number of 32-bit words to program
 *
 * @return       0 for success, non-zero for failure
 *               1 Invalid Key
 *               2 Invalid destination start address
 *               3 Invalid destination end address
 *               4 Invalid source buffer start address
 *               5 Invalid source buffer end address
 *               6 Hardware error
 *               7 Reserved
 *               8 Hardware error
 *               9 OTP interrupts detected
 *              10 Memory not powered on
 *              11 Area is write protected
 *              12 Area is copy protected
 *              13 Internal error
 *
 *****************************************************************************/
int
nv_program_main2(uint32_t  value,
                 uint32_t  Program_nWipe,
                 uint32_t  Addr_WipeData,
                 uint32_t  WordOffset,
                 uint32_t  NumberOfWords);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_BOOTROM_HELPER_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

