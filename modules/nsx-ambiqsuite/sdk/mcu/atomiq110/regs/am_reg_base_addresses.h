//*****************************************************************************
//
//  am_reg_base_addresses.h
//! @file am_reg_base_addresses.h
//!
//! @brief Register defines for all module base addresses
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
#ifndef AM_REG_BASE_ADDRESSES_H
#define AM_REG_BASE_ADDRESSES_H

#include "stdint.h"

//*****************************************************************************
// Non Secure memory addresses
//*****************************************************************************
//
// DTCM address space
//
#define DTCM_BASEADDR                           (0x20000000UL)
#define DTCM_MAX_SIZE                           (512UL * 1024UL)

//
// The SSRAM space
//
#define SSRAM_BASEADDR                          (0x21000000)
#define SSRAM_MAX_SIZE                          (3072UL * 1024UL)

//
// The HBLRAM space
//
#define HBLRAM_BASEADDR                         (0x60000000)
#define HBLRAM_MAX_SIZE                         (13312UL * 1024UL)

//
// The RRAM space
//
#define RRAM_BASEADDR                           (0x22000000UL)
#define RRAM_MAX_SIZE                           (4096UL * 1024UL)

//
// Virtual space
//
#define VIRTUAL_BASEADDR                        (0x08000000UL)
#define VIRTUAL_MAX_SIZE                        (128UL * 1024UL)
#define VIRTUAL_MAX_ADDR                        (0x0FFFFFFF)

// ****************************************************************************
// Secure memory addresses
// ****************************************************************************
//
// DTCM address space
//
#define DTCM_BASEADDR_SECURE                    (0x30000000UL)
#define DTCM_MAX_SIZE_SECURE                    (512UL * 1024UL)

//
// The SSRAM space
//
#define SSRAM_BASEADDR_SECURE                   (0x31000000)
#define SSRAM_MAX_SIZE_SECURE                   (3072UL * 1024UL)

//
// The RRAM space
//
#define RRAM_BASEADDR_SECURE                    (0x32000000UL)
#define RRAM_MAX_SIZE_SECURE                    (4096UL * 1024UL)


// ****************************************************************************
// INFO base addresses
// ****************************************************************************
#define AM_REG_INFO0_BASEADDR           0x42000000
#define AM_REG_INFO1_BASEADDR           0x42002000

// #### INTERNAL BEGIN ####
// Note: AM_REG_OTP_INFOn_BASEADDR are defined in am_mcu_atomiq110_otpinfo(n).h.
//#define AM_REG_OTP_INFO0_BASEADDR       0x42004000
//#define AM_REG_OTP_INFO1_BASEADDR       0x42006000
//#define AM_REG_OTP_INFOC_BASEADDR       0x400C2000
// #### INTERNAL END ####
//
// INFO sizes
//
#define AM_REG_INFO0_SIZE               2048
#define AM_REG_INFO1_SIZE               6144
#define AM_REG_INFOC_SIZE               8192
#define AM_REG_OTP_INFO0_SIZE           256
#define AM_REG_OTP_INFO1_SIZE           2816
#define AM_REG_OTP_INFOC_SIZE           1024



// ****************************************************************************
// XSPI aperture address ranges
// ****************************************************************************
// #### INTERNAL BEGIN ####
// Information about the XSPI aperatures comes from the Confluence page Carrera
// Specifications (Carrera)->CPU Subsystem, Oct 2025.
// #### INTERNAL END ####
//
// Please note that the END_ADDR as used here is actually the (end address + 1).
//
#define XSPI0_XIP_BASEADDR                      (0x80000000UL)
#define XSPI1_XIP_BASEADDR                      (0xA0000000UL)
#define XSPI2_XIP_BASEADDR                      (0xC0000000UL)

#define XSPI0_XIP_BASEADDR_SECURE               (0x90000000UL)
#define XSPI1_XIP_BASEADDR_SECURE               (0xB0000000UL)
#define XSPI2_XIP_BASEADDR_SECURE               (0xD0000000UL)

#define XSPI0_APERTURE_START_ADDR               (0x80000000UL)
#define XSPI0_APERTURE_END_ADDR                 (0x90000000UL)
#define XSPI0_APERTURE_START_SECURE_ADDR        (0x90000000UL)
#define XSPI0_APERTURE_END_SECURE_ADDR          (0xA0000000UL)
#define XSPI1_APERTURE_START_ADDR               (0xA0000000UL)
#define XSPI1_APERTURE_END_ADDR                 (0xB0000000UL)
#define XSPI1_APERTURE_START_SECURE_ADDR        (0xB0000000UL)
#define XSPI1_APERTURE_END_SECURE_ADDR          (0xC0000000UL)
#define XSPI2_APERTURE_START_ADDR               (0xC0000000UL)
#define XSPI2_APERTURE_END_ADDR                 (0xD0000000UL)
#define XSPI2_APERTURE_START_SECURE_ADDR        (0xD0000000UL)
#define XSPI2_APERTURE_END_SECURE_ADDR          (0xE0000000UL)
#endif // AM_REG_BASE_ADDRESSES_H
