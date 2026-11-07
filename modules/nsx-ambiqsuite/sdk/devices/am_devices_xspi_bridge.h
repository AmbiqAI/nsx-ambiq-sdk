//*****************************************************************************
//
//! @file am_devices_xspi_bridge.h
//!
//! @brief Atomiq110 xSPI bridge device driver for XSPIMC.
//
//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_DEVICES_XSPI_BRIDGE_H
#define AM_DEVICES_XSPI_BRIDGE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "am_devices_xspi.h"

#define XSPI_BRIDGE_STS_SYNC_Pos                    24
#define XSPI_BRIDGE_STS_SYNC_Msk                    (0xFFu << XSPI_BRIDGE_STS_SYNC_Pos)
#define XSPI_BRIDGE_STS_DUMMY_Pos                   16
#define XSPI_BRIDGE_STS_DUMMY_Msk                   (0xFFu << XSPI_BRIDGE_STS_DUMMY_Pos)
// #### INTERNAL BEGIN ####
#define XSPI_BRIDGE_STS_DQSEN_Pos                   15
#define XSPI_BRIDGE_STS_DQSEN_Msk                   (0x1u << XSPI_BRIDGE_STS_DQSEN_Pos)
// #### INTERNAL END ####
#define XSPI_BRIDGE_STS_CMODE_Pos                   12
#define XSPI_BRIDGE_STS_CMODE_Msk                   (0x3u << XSPI_BRIDGE_STS_CMODE_Pos)
#define XSPI_BRIDGE_STS_AMODE_Pos                   10
#define XSPI_BRIDGE_STS_AMODE_Msk                   (0x3u << XSPI_BRIDGE_STS_AMODE_Pos)
#define XSPI_BRIDGE_STS_DMODE_Pos                   8
#define XSPI_BRIDGE_STS_DMODE_Msk                   (0x3u << XSPI_BRIDGE_STS_DMODE_Pos)
#define XSPI_BRIDGE_STS_BURSTERR_Msk                (0x1u << 7)
#define XSPI_BRIDGE_STS_OVERRUN_Msk                 (0x1u << 6)
#define XSPI_BRIDGE_STS_UNDERRUN_Msk                (0x1u << 5)
#define XSPI_BRIDGE_STS_BUSERR_Msk                  (0x1u << 4)
#define XSPI_BRIDGE_STS_INVLEN_Msk                  (0x1u << 3)
#define XSPI_BRIDGE_STS_INVCMD_Msk                  (0x1u << 2)
#define XSPI_BRIDGE_STS_WRITE_Msk                   (0x1u << 1)
#define XSPI_BRIDGE_STS_BUSY_Msk                    (0x1u << 0)

#define XSPI_BRIDGE_STS_WRITABLE_MASK               (XSPI_BRIDGE_STS_BURSTERR_Msk | \
                                                     XSPI_BRIDGE_STS_OVERRUN_Msk  | \
                                                     XSPI_BRIDGE_STS_UNDERRUN_Msk | \
                                                     XSPI_BRIDGE_STS_BUSERR_Msk   | \
                                                     XSPI_BRIDGE_STS_INVLEN_Msk   | \
                                                     XSPI_BRIDGE_STS_INVCMD_Msk)
#define XSPI_BRIDGE_STS_RESET_VALUE                 0xA5FF0000u
#define XSPI_BRIDGE_DUMMY_MIN                       4u
#define XSPI_BRIDGE_DUMMY_MAX                       255u
#define AM_DEVICES_XSPI_BRIDGE_IP_VERSION           0x00090100u

#define AM_DEVICES_XSPI_BRIDGE_SOFT_RESET           0x99
#define AM_DEVICES_XSPI_BRIDGE_WRITE_1BYTE          0x60
#define AM_DEVICES_XSPI_BRIDGE_WRITE_2BYTES         0x61
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL1            0x70
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL2            0x71
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL4            0x72
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL8            0x73
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL16           0x74
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL32           0x75
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL64           0x76
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL128          0x77
#define AM_DEVICES_XSPI_BRIDGE_WRITE_BL256          0x78
#define AM_DEVICES_XSPI_BRIDGE_WRITE_UL             0x7F

#define AM_DEVICES_XSPI_BRIDGE_READ_1BYTE           0x80
#define AM_DEVICES_XSPI_BRIDGE_READ_2BYTES          0x81
#define AM_DEVICES_XSPI_BRIDGE_READ_BL1             0xA0
#define AM_DEVICES_XSPI_BRIDGE_READ_BL2             0xA1
#define AM_DEVICES_XSPI_BRIDGE_READ_BL4             0xA2
#define AM_DEVICES_XSPI_BRIDGE_READ_BL8             0xA3
#define AM_DEVICES_XSPI_BRIDGE_READ_BL16            0xA4
#define AM_DEVICES_XSPI_BRIDGE_READ_BL32            0xA5
#define AM_DEVICES_XSPI_BRIDGE_READ_BL64            0xA6
#define AM_DEVICES_XSPI_BRIDGE_READ_BL128           0xA7
#define AM_DEVICES_XSPI_BRIDGE_READ_BL256           0xA8
#define AM_DEVICES_XSPI_BRIDGE_READ_UL              0xC0

#define AM_DEVICES_XSPI_BRIDGE_READ_STS             0x10
#define AM_DEVICES_XSPI_BRIDGE_READ_ADR             0x11
#define AM_DEVICES_XSPI_BRIDGE_READ_RXD             0x12
#define AM_DEVICES_XSPI_BRIDGE_READ_TXD             0x13
#define AM_DEVICES_XSPI_BRIDGE_READ_CFG             0x15
#define AM_DEVICES_XSPI_BRIDGE_READ_VER             0x16
#define AM_DEVICES_XSPI_BRIDGE_WRITE_STS            0x14
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_1_1           0x20
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_2_2           0x21
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_4_4           0x22
// #### INTERNAL BEGIN ####
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_8_8           0x23
// #### INTERNAL END ####
#define AM_DEVICES_XSPI_BRIDGE_SOPM_2_2_2           0x24
#define AM_DEVICES_XSPI_BRIDGE_SOPM_4_4_4           0x25
// #### INTERNAL BEGIN ####
#define AM_DEVICES_XSPI_BRIDGE_SOPM_8_8_8           0x26
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_4_4_DQS       0x27
#define AM_DEVICES_XSPI_BRIDGE_SOPM_1_8_8_DQS       0x28
#define AM_DEVICES_XSPI_BRIDGE_SOPM_4_4_4_DQS       0x29
#define AM_DEVICES_XSPI_BRIDGE_SOPM_8_8_8_DQS       0x2A
// #### INTERNAL END ####
#define AM_DEVICES_XSPI_BRIDGE_SET_DUMMY            0x40

#define AM_DEVICES_XSPI_BRIDGE_MAX_PACKET_SIZE      (256UL * 1024UL * 1024UL)

typedef union
{
    struct
    {
        uint32_t BUSY       : 1;
        uint32_t WRITE      : 1;
        uint32_t INVCMD     : 1;
        uint32_t INVLEN     : 1;
        uint32_t BUSERR     : 1;
        uint32_t UNDERRUN   : 1;
        uint32_t OVERRUN    : 1;
        uint32_t BURSTERR   : 1;
        uint32_t DMODE      : 2;
        uint32_t AMODE      : 2;
        uint32_t CMODE      : 2;
        uint32_t RESERVED0  : 2;
        uint32_t DUMMY      : 8;
        uint32_t SYNC       : 8;
    } STS_b;
    uint32_t STS;
} am_devices_xspi_bridge_status_reg_t;

#define AM_DEVICES_XSPI_BRIDGE_GLOBAL_RAM_SIZE 96u

extern const am_device_xspi_func_t apiXSPIBridge;

#define AM_DEVICE_XSPI_BRIDGE_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM, XSPI_HANDLE) \
        AM_DEVICE_XSPI_DEV_HANDLE_DEFINE(ATTR, MODULE_NAME, CE_NUM,                       \
                                         AM_DEVICES_XSPI_BRIDGE_GLOBAL_RAM_SIZE,   \
                                         apiXSPIBridge, XSPI_HANDLE)

#ifdef __cplusplus
}
#endif

#endif // AM_DEVICES_XSPI_BRIDGE_H
