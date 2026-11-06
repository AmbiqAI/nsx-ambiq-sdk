//*****************************************************************************
//
//  am_bsp_xspi_dev.h
//! @file
//!
//! @brief Functions to aid on board xSPI device configuration.
//!
//! @addtogroup BSP Board Support Package (BSP)
//! @addtogroup atomiq110_fpga_turbo BSP
//! @ingroup BSP
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_BSP_XSPI_DEV_H
#define AM_BSP_XSPI_DEV_H

#include "am_devices_xspi.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef AM_BSP_BOARD_VERSION
#define AM_BSP_BOARD_VERSION 1
#endif

typedef struct
{
    const char                      name[16];        // Device part name.
    uint32_t                        ui32Designator;  // Schematic U number.
    void                           *pXSPIHandle;     // xSPI module handle.
    void                           *pDevHandle;      // Device handle.
    am_devices_xspi_type_e          eDevice;         // Device type.
    const am_devices_xspi_config_t *pDevConfig;      // Device specific configuration.
} am_bsp_xspi_device_info_t;

//****************************************************************************
//
// XSPI Device definitions.
//
//****************************************************************************
#if AM_BSP_BOARD_VERSION == 1

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_ATXP032
#define AM_DEVICES_XSPI_NOR_ENABLE_ATXP032 0
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064
#define AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064 0
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G
#define AM_DEVICES_XSPI_NOR_ENABLE_MX25UM51245G 0
#endif


#define AM_BSP_XSPI_DEVICE_COUNT           5
extern const am_bsp_xspi_device_info_t sBSPXSPIDevices[AM_BSP_XSPI_DEVICE_COUNT];

//
// Fill with Turbo 1 hardware
//
#define AM_BSP_XSPI_DEV_APS25616N_IDX   0u
#define AM_BSP_XSPI_DEV_W958D6NW_IDX    1u
// Turbo 1
#define AM_BSP_XSPI_DEV_NOR_U22_IDX     2u
#define AM_BSP_XSPI_DEV_NOR_U24_IDX     3u
#define AM_BSP_XSPI_DEV_BRIDGE_IDX      4u

#define AM_BSP_XSPI_FOR_EACH_APS25616N(M)    \
        M(AM_BSP_XSPI_DEV_APS25616N_IDX)

#define AM_BSP_XSPI_FOR_EACH_W958D6NW(M)     \
        M(AM_BSP_XSPI_DEV_W958D6NW_IDX)

#define AM_BSP_XSPI_FOR_EACH_ATXP032(M)      \
        M(AM_BSP_XSPI_DEV_NOR_U22_IDX)       \
        M(AM_BSP_XSPI_DEV_NOR_U24_IDX)

#define AM_BSP_XSPI_FOR_EACH_IS25WX064(M)    \
        M(AM_BSP_XSPI_DEV_NOR_U22_IDX)       \
        M(AM_BSP_XSPI_DEV_NOR_U24_IDX)

#define AM_BSP_XSPI_FOR_EACH_MX25UM51245G(M) \
        M(AM_BSP_XSPI_DEV_NOR_U22_IDX)       \
        M(AM_BSP_XSPI_DEV_NOR_U24_IDX)

/****************************************************************************************/
#elif AM_BSP_BOARD_VERSION == 2

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064
#define AM_DEVICES_XSPI_NOR_ENABLE_IS25WX064 0
#endif

#ifndef AM_DEVICES_XSPI_NOR_ENABLE_W25Q25
#define AM_DEVICES_XSPI_NOR_ENABLE_W25Q25 0
#endif

#ifndef AM_DEVICES_XSPI_NAND_ENABLE_W25N02KW
#define AM_DEVICES_XSPI_NAND_ENABLE_W25N02KW 0
#endif

#define AM_BSP_XSPI_DEVICE_COUNT           6
extern const am_bsp_xspi_device_info_t sBSPXSPIDevices[AM_BSP_XSPI_DEVICE_COUNT];

//
// Fill with Turbo 2 hardware
//
#define AM_BSP_XSPI_DEV_APS25616N_IDX   0u
#define AM_BSP_XSPI_DEV_W958D6NW_IDX    1u
#define AM_BSP_XSPI_DEV_IS25WX064_IDX   2u
#define AM_BSP_XSPI_DEV_W25Q25_IDX      3u
#define AM_BSP_XSPI_DEV_W25N02KW_IDX    4u
#define AM_BSP_XSPI_DEV_BRIDGE_IDX      5u

#define AM_BSP_XSPI_FOR_EACH_APS25616N(M)    \
        M(AM_BSP_XSPI_DEV_APS25616N_IDX)

#define AM_BSP_XSPI_FOR_EACH_W958D6NW(M)     \
        M(AM_BSP_XSPI_DEV_W958D6NW_IDX)

#define AM_BSP_XSPI_FOR_EACH_IS25WX064(M)    \
        M(AM_BSP_XSPI_DEV_IS25WX064_IDX)

#define AM_BSP_XSPI_FOR_EACH_W25Q25(M)       \
        M(AM_BSP_XSPI_DEV_W25Q25_IDX)

#define AM_BSP_XSPI_FOR_EACH_W25N02KW(M)     \
        M(AM_BSP_XSPI_DEV_W25N02KW_IDX)

#else
#error "Board version not supported!!"
#endif //AM_BSP_BOARD_VERSION

#ifndef AM_DEVICES_XSPI_BRIDGE_ENABLE
#define AM_DEVICES_XSPI_BRIDGE_ENABLE 0
#endif

#ifndef AM_BSP_XSPI_BRIDGE_HOST_MODULE
#define AM_BSP_XSPI_BRIDGE_HOST_MODULE 2
#endif

#define AM_BSP_XSPI_FOR_EACH_BRIDGE(M) \
        M(AM_BSP_XSPI_DEV_BRIDGE_IDX)

// Utility
#define AM_BSP_XSPI_FOR_EACH_(name, M)  AM_BSP_XSPI_FOR_EACH_##name(M)
#define AM_BSP_XSPI_FOR_EACH(name, M)   AM_BSP_XSPI_FOR_EACH_(name, M)
#define AM_BSP_XSPI_DEV_AT(idx)         (&sBSPXSPIDevices[(idx)])
#define AM_BSP_XSPI_MATCH_INFO(idx)     AM_BSP_XSPI_DEV_AT(idx),

#define AM_BSP_XSPI_DEV(name) \
    ((const am_bsp_xspi_device_info_t *[]){ AM_BSP_XSPI_FOR_EACH(name, AM_BSP_XSPI_MATCH_INFO) NULL })

#define AM_BSP_XSPI_DEV_HANDLE(info) \
    ((am_device_xspi_periph_handle_t *)(info)->pDevHandle)

#define AM_BSP_XSPI_HANDLE(info) \
    ((am_hal_periph_handle_t *)(info)->pXSPIHandle)

#define AM_BSP_XSPI_DEV_CONFIG(info) \
    ((const am_devices_xspi_config_t *)(info)->pDevConfig)


#ifdef __cplusplus
}
#endif

#endif // AM_BSP_XSPI_DEV_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
