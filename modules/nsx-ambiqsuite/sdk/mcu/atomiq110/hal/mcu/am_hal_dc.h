//*****************************************************************************
//
//! @file am_hal_dc.h
//!
//! @brief Functions and definitions for the Display Controller (DC) HAL.
//!
//! This module provides an interface for configuring and controlling the
//! Display Controller hardware. It supports various interface types including
//! SPI, QSPI, DSPI, DPI, DBI, and JDI.
//!
//! Typical bring-up sequence:
//!   1. am_hal_dc_initialize()   - obtain a module handle
//!   2. am_hal_dc_control(SET_CLK) - optional CRM clock selection (before power-on)
//!   3. am_hal_dc_power_control(WAKE) - enable clocks and peripheral power
//!   4. am_hal_dc_config()       - select interface, timing, and pixel format
//!   5. am_hal_dc_layer_set() / am_hal_dc_layer_enable() - configure compositing layers
//!   6. am_hal_dc_blocking_transfer() or am_hal_dc_nonblocking_transfer() - send data
//!   7. am_hal_dc_interrupt_service() - dispatch callbacks from the DC ISR
//!
//! @addtogroup dc_atomiq110 DC Functionality
//! @ingroup atomiq110_hal
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
#ifndef AM_HAL_DC_H
#define AM_HAL_DC_H

#include <stdint.h>
#include <stdbool.h>

#include "hal/am_hal_sysctrl.h"
#include "nema_dc_jdi.h"

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Macro definitions
// Prefer CMSIS names from atomiq110.h (DC_*). Local literals only when CMSIS
// does not define an equivalent.
//
//*****************************************************************************

#define AM_HAL_DC_INT_SPICMD                            DC_INTERRUPT_SPICMDINTER_Msk
#define AM_HAL_DC_INT_SPIFRAMEEND                       DC_INTERRUPT_SPIFRAMEENDINTER_Msk
#define AM_HAL_DC_INT_FRAMEEND                          DC_INTERRUPT_FRAMEENDINTER_Msk
#define AM_HAL_DC_INT_TE                                DC_INTERRUPT_TEINTER_Msk
#define AM_HAL_DC_INT_TETRIG                            DC_INTERRUPT_TETRIG_Msk
#define AM_HAL_DC_INT_VSYNC                             DC_INTERRUPT_VSYNCINTER_Msk
#define AM_HAL_DC_INT_HSYNC                             DC_INTERRUPT_HSYNCINTER_Msk

#define AM_HAL_DC_FRAME_END                             (AM_HAL_DC_INT_SPIFRAMEEND | AM_HAL_DC_INT_FRAMEEND)
#define AM_HAL_DC_INT_ALL                               (AM_HAL_DC_INT_SPICMD | AM_HAL_DC_INT_SPIFRAMEEND | \
                                                         AM_HAL_DC_INT_FRAMEEND | AM_HAL_DC_INT_TE |        \
                                                         AM_HAL_DC_INT_TETRIG | AM_HAL_DC_INT_VSYNC | AM_HAL_DC_INT_HSYNC)

//! Number of DC compositing layers.
#define AM_HAL_DC_LAYERS_NUMBER                         (2)

//! MIPI DCS command identifiers (MIPI DCS opcodes).
//! Use these as am_hal_dc_transfer_t::ui8Command for register-oriented transfers.
#define AM_HAL_DC_CMD_TYPE_ENTER_IDLE_MODE              (0x39U)
#define AM_HAL_DC_CMD_TYPE_ENTER_INVERT_MODE            (0x21U)
#define AM_HAL_DC_CMD_TYPE_ENTER_NORMAL_MODE            (0x13U)
#define AM_HAL_DC_CMD_TYPE_ENTER_PARTIAL_MODE           (0x12U)
#define AM_HAL_DC_CMD_TYPE_ENTER_SLEEP_MODE             (0x10U)
#define AM_HAL_DC_CMD_TYPE_EXIT_IDLE_MODE               (0x38U)
#define AM_HAL_DC_CMD_TYPE_EXIT_INVERT_MODE             (0x20U)
#define AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE              (0x11U)
#define AM_HAL_DC_CMD_TYPE_GET_3D_CONTROL               (0x3FU)
#define AM_HAL_DC_CMD_TYPE_GET_ADDRESS_MODE             (0x0BU)
#define AM_HAL_DC_CMD_TYPE_GET_BLUE_CHANNEL             (0x08U)
#define AM_HAL_DC_CMD_TYPE_DIAGNOSTIC_RESULT            (0x0FU)
#define AM_HAL_DC_CMD_TYPE_DISPLAY_MODE                 (0x0DU)
#define AM_HAL_DC_CMD_TYPE_GET_GREEN_CHANNEL            (0x07U)
#define AM_HAL_DC_CMD_TYPE_GET_PIXEL_FORMAT             (0x0CU)
#define AM_HAL_DC_CMD_TYPE_GET_POWER_MODE               (0x0AU)
#define AM_HAL_DC_CMD_TYPE_GET_RED_CHANNEL              (0x06U)
#define AM_HAL_DC_CMD_TYPE_GET_SCANLINE                 (0x45U)
#define AM_HAL_DC_CMD_TYPE_GET_SIGNAL_MODE              (0x0EU)
#define AM_HAL_DC_CMD_TYPE_NOP                          (0x00U)
#define AM_HAL_DC_CMD_TYPE_READ_DDB_CONTINUE            (0xA8U)
#define AM_HAL_DC_CMD_TYPE_READ_DDB_START               (0xA1U)
#define AM_HAL_DC_CMD_TYPE_READ_MEMORY_CONTINUE         (0x3EU)
#define AM_HAL_DC_CMD_TYPE_READ_MEMORY_START            (0x2EU)
#define AM_HAL_DC_CMD_TYPE_SET_3D_CONTROL               (0x3DU)
#define AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE             (0x36U)
#define AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS           (0x2AU)
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_OFF              (0x28U)
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON               (0x29U)
#define AM_HAL_DC_CMD_TYPE_SET_GAMMA_CURVE              (0x26U)
#define AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS             (0x2BU)
#define AM_HAL_DC_CMD_TYPE_SET_PARTIAL_COLUMNS          (0x31U)
#define AM_HAL_DC_CMD_TYPE_SET_PARTIAL_ROWS             (0x30U)
#define AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT             (0x3AU)
#define AM_HAL_DC_CMD_TYPE_SET_SCROLL_AREA              (0x33U)
#define AM_HAL_DC_CMD_TYPE_SET_SCROLL_START             (0x37U)
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_OFF                 (0x34U)
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_ON                  (0x35U)
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE            (0x44U)
#define AM_HAL_DC_CMD_TYPE_SET_VSYNC_TIMING             (0x40U)
#define AM_HAL_DC_CMD_TYPE_SET_SOFT_RESET               (0x01U)
#define AM_HAL_DC_CMD_TYPE_WRITE_LUT                    (0x2DU)
#define AM_HAL_DC_CMD_TYPE_WRITE_MEMORY_CONTINUE        (0x3CU)
#define AM_HAL_DC_CMD_TYPE_WRITE_MEMORY_START           (0x2CU)
#define AM_HAL_DC_CMD_TYPE_SNAPSHOT                     (0xFFU)
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS       (0x51U)
#define AM_HAL_DC_CMD_TYPE_GET_DISPLAY_BRIGHTNESS       (0x52U)

//! DBI/SPI INTERFACECFG width field (DC_INTERFACECFG_INTERFACEWIDTH).
#define AM_HAL_DC_IF_CFG_DBI8                           (0U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)
#define AM_HAL_DC_IF_CFG_DBI9                           (1U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)
#define AM_HAL_DC_IF_CFG_DBI16                          (2U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)
#define AM_HAL_DC_IF_CFG_SPI                            (3U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)
#define AM_HAL_DC_IF_CFG_DSPI                           (4U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)
#define AM_HAL_DC_IF_CFG_QSPI                           (5U << DC_INTERFACECFG_INTERFACEWIDTH_Pos)

//! DBI/SPI INTERFACECFG color-format option field.
#define AM_HAL_DC_IF_OPTION0                            (0U << DC_INTERFACECFG_COLORFORMATOPTION_Pos)
#define AM_HAL_DC_IF_OPTION1                            (1U << DC_INTERFACECFG_COLORFORMATOPTION_Pos)
#define AM_HAL_DC_IF_OPTION2                            (2U << DC_INTERFACECFG_COLORFORMATOPTION_Pos)
#define AM_HAL_DC_IF_OPTION3                            (3U << DC_INTERFACECFG_COLORFORMATOPTION_Pos)
#define AM_HAL_DC_IF_OPTION4                            (4U << DC_INTERFACECFG_COLORFORMATOPTION_Pos)

//! DPI MODE color formats (DC_MODE_DPICOLORFORMATS / EXTRADPICOLORFORMATS).
#define AM_HAL_DC_FORMAT_DPI_RGB888                     (0U << DC_MODE_EXTRADPICOLORFORMATS_Pos)
// #### INTERNAL BEGIN ####
// DPI gearbox don't support RGB666 on AT110.
#define AM_HAL_DC_FORMAT_DPI_RGB666_CFG1                (1U << DC_MODE_DPICOLORFORMATS_Pos)
#define AM_HAL_DC_FORMAT_DPI_RGB666_CFG2                (2U << DC_MODE_DPICOLORFORMATS_Pos)
// #### INTERNAL END ####
#define AM_HAL_DC_FORMAT_DPI_RGB565_CFG1                (3U << DC_MODE_DPICOLORFORMATS_Pos)
#define AM_HAL_DC_FORMAT_DPI_RGB565_CFG2                (4U << DC_MODE_DPICOLORFORMATS_Pos)
#define AM_HAL_DC_FORMAT_DPI_RGB565_CFG3                (5U << DC_MODE_DPICOLORFORMATS_Pos)

//! DC MODE control bits.
#define AM_HAL_DC_MODE_ENABLE                           DC_MODE_NEMADCEN_Msk
#define AM_HAL_DC_MODE_ONE_FRAME                        DC_MODE_SINGLEFRAME_Msk
#define AM_HAL_DC_MODE_OUTP_OFF                         DC_MODE_ENOUTPUTCOLORFORMATS_Msk
#define AM_HAL_DC_MODE_SCANDOUBLE                       DC_MODE_DOUBLESCANEN_Msk
#define AM_HAL_DC_MODE_MIP_IF                           (4U << DC_MODE_EXTRADPICOLORFORMATS_Pos)

//! DC INTERFACECFG control bits.
#define AM_HAL_DC_IFCFG_EN                              DC_INTERFACECFG_ENINTERFACES_Msk
#define AM_HAL_DC_IFCFG_FRC_CSX                         DC_INTERFACECFG_SWCTRLCS_Msk
#define AM_HAL_DC_IFCFG_CSX_V                           DC_INTERFACECFG_SWCTRLCSVALUE_Msk
#define AM_HAL_DC_IFCFG_TE                              DC_INTERFACECFG_ENABLETE_Msk
#define AM_HAL_DC_IFCFG_SPIDC_DQSPI                     DC_INTERFACECFG_SPISD1ASSPIDC_Msk
#define AM_HAL_DC_IFCFG_RESX                            DC_INTERFACECFG_RSTSIGNAL_Msk
#define AM_HAL_DC_IFCFG_SPI3                            (2U << DC_INTERFACECFG_SPI3SPI4_Pos)
#define AM_HAL_DC_IFCFG_SPI4                            (1U << DC_INTERFACECFG_SPI3SPI4_Pos)
#define AM_HAL_DC_IFCFG_EN_STALL                        DC_INTERFACECFG_ENDBIBBACKPRESSURE_Msk
#define AM_HAL_DC_IFCFG_SPI_CPHA                        DC_INTERFACECFG_SPICPHASE_Msk
#define AM_HAL_DC_IFCFG_SPI_CPOL                        DC_INTERFACECFG_SPICPOL_Msk
#define AM_HAL_DC_IFCFG_EN_DVALID                       DC_INTERFACECFG_DBIBVALID_Msk
#define AM_HAL_DC_IFCFG_SPI_HOLD                        DC_INTERFACECFG_HOLDCMDPARAMS_Msk
#define AM_HAL_DC_IFCFG_EXT_CTRL                        DC_INTERFACECFG_EXTRNCTRL_Msk
#define AM_HAL_DC_IFCFG_BLANKING_EN                     DC_INTERFACECFG_HORBLANKING_Msk
#define AM_HAL_DC_IFCFG_DSPI_SPIX                       DC_INTERFACECFG_DSPISUBPIX_Msk
#define AM_HAL_DC_IFCFG_DSPI                            (1U << DC_INTERFACECFG_SPIMODE_Pos)
#define AM_HAL_DC_IFCFG_QSPI                            (2U << DC_INTERFACECFG_SPIMODE_Pos)
#define AM_HAL_DC_IFCFG_QSPI_DDR                        (3U << DC_INTERFACECFG_SPIMODE_Pos)

//! DC INTERFACECMD bits.
#define AM_HAL_DC_IFCMD_DBIB                            DC_INTERFACECMD_DATACMD_Msk
#define AM_HAL_DC_IFCMD_WIDTH16                         (1U << DC_INTERFACECMD_CMDWIDTH_Pos)
#define AM_HAL_DC_IFCMD_WIDTH24                         (2U << DC_INTERFACECMD_CMDWIDTH_Pos)
#define AM_HAL_DC_IFCMD_QSPI_MASK                       DC_INTERFACECMD_SERIALCMD_Msk
#define AM_HAL_DC_IFCMD_READ                            DC_INTERFACECMD_READMODE_Msk
#define AM_HAL_DC_IFCMD_EXT_CTRL                        DC_INTERFACECMD_DBIBSUPPORTSIGNALS_Msk

//! DC CLKCTRLCG bits.
#define AM_HAL_DC_CLKCTRLCG_CLK_SWAP                    DC_CLKCTRLCG_PIXFORMATCLKSWAP_Msk

//! QSPI serial command helpers (payload nibbles in INTERFACECMD data).
#define AM_HAL_DC_QSPI_CMD_OFFSET                       (8U)
#define AM_HAL_DC_QSPI_CMD1_DATA1                       (2U)
#define AM_HAL_DC_QSPI_CMD1_DATA4                       ((1U << 5) | (1U << 4) | 2U)
#define AM_HAL_DC_QSPI_CMD1_RDAT1                       (3U)

//! DC status bits (CMSIS DC_STATUS_*).
#define AM_HAL_DC_STATUS_GPI_STUCK                      DC_STATUS_GPISTACK_Msk
#define AM_HAL_DC_STATUS_CRC_READY                      DC_STATUS_CRCREADY_Msk
#define AM_HAL_DC_STATUS_DBI_RD_WR_ON                   DC_STATUS_SPIRDWR_Msk
#define AM_HAL_DC_STATUS_DBI_CMD_READY                  DC_STATUS_CMDFIFOACK_Msk
#define AM_HAL_DC_STATUS_DBI_CS                         DC_STATUS_CHIPSELECT_Msk
#define AM_HAL_DC_STATUS_FRAME_END                      DC_STATUS_FRAMEEND_Msk
#define AM_HAL_DC_STATUS_DBI_PENDING_TRANS              DC_STATUS_PENDINGTRANSACTION_Msk
#define AM_HAL_DC_STATUS_DBI_PENDING_CMD                DC_STATUS_PENDINGCMD_Msk
#define AM_HAL_DC_STATUS_DBI_PENDING_DATA               DC_STATUS_PENDINGPIXEL_Msk
#define AM_HAL_DC_STATUS_TE                             DC_STATUS_TE_Msk
#define AM_HAL_DC_STATUS_STICKY                         DC_STATUS_UNDERFLOWSTICKYBIT_Msk
#define AM_HAL_DC_STATUS_UNDERFLOW                      DC_STATUS_UNDERFLOW_Msk
#define AM_HAL_DC_STATUS_DPI_CSYNC                      DC_STATUS_CSYNC_Msk
#define AM_HAL_DC_STATUS_VSYNC_TE                       DC_STATUS_VSYNCORTE_Msk
#define AM_HAL_DC_STATUS_HSYNC                          DC_STATUS_HSYNC_Msk
#define AM_HAL_DC_STATUS_FRAMEGEN_BUSY                  DC_STATUS_DATAEN_Msk
#define AM_HAL_DC_STATUS_ACTIVE                         DC_STATUS_VSYNC_Msk

#define AM_HAL_DC_STATUS_DBI_BUSY                       (AM_HAL_DC_STATUS_DBI_RD_WR_ON       | \
                                                        AM_HAL_DC_STATUS_DBI_CS             | \
                                                        AM_HAL_DC_STATUS_FRAME_END          | \
                                                        AM_HAL_DC_STATUS_DBI_PENDING_TRANS  | \
                                                        AM_HAL_DC_STATUS_DBI_PENDING_CMD    | \
                                                        AM_HAL_DC_STATUS_DBI_PENDING_DATA   )
//*****************************************************************************
//
// Type definitions
//
//*****************************************************************************
//
//! Panel pixel output formats.
//
typedef enum
{
// #### INTERNAL BEGIN ####
    AM_HAL_DC_FORMAT_RGB111 = 1U,
// #### INTERNAL END ####
    // output format only supported by DBI-B and SPI interfaces
    AM_HAL_DC_FORMAT_RGB332 = 2U,
    // output format only supported by DBI-B, SPI, DSPI and QSPI interfaces
    AM_HAL_DC_FORMAT_RGB444 = 3U,
// #### INTERNAL BEGIN ####
    AM_HAL_DC_FORMAT_RGBBW  = 4U,
// #### INTERNAL END ####
    // output format only supported by DBI-B, SPI, DSPI and QSPI interfaces
    AM_HAL_DC_FORMAT_RGB565 = 5U,
    // output format only supported by DBI-B, SPI, DSPI and QSPI interfaces
    AM_HAL_DC_FORMAT_RGB666 = 6U,
    // output format only supported by DBI-B, SPI, DSPI and QSPI interfaces
    AM_HAL_DC_FORMAT_RGB888 = 7U,
} am_hal_dc_output_format_e;
//
//! Layer input color formats (NemaDC layer format codes).
//
typedef enum
{
    // 8-bit formats
    AM_HAL_DC_LAYER_FORMAT_LUT8         = 0x00U,
    AM_HAL_DC_LAYER_FORMAT_A8           = 0x1EU,
    AM_HAL_DC_LAYER_FORMAT_L8           = 0x07U,
    AM_HAL_DC_LAYER_FORMAT_AL44         = 0x19U,
    AM_HAL_DC_LAYER_FORMAT_RGB332       = 0x04U,
    AM_HAL_DC_LAYER_FORMAT_RGBA2222     = 0x1AU,
    AM_HAL_DC_LAYER_FORMAT_BGRA2222     = 0x1BU,
    AM_HAL_DC_LAYER_FORMAT_ARGB2222     = 0x1CU,
    AM_HAL_DC_LAYER_FORMAT_ABGR2222     = 0x1DU,

    // 16-bit formats
    AM_HAL_DC_LAYER_FORMAT_RGB565       = 0x05U,
    AM_HAL_DC_LAYER_FORMAT_BGR565       = 0x16U,
    AM_HAL_DC_LAYER_FORMAT_AL88         = 0x17U,
    AM_HAL_DC_LAYER_FORMAT_RGBA4444     = 0x15U,
    AM_HAL_DC_LAYER_FORMAT_BGRA4444     = 0x0AU,
    AM_HAL_DC_LAYER_FORMAT_ARGB4444     = 0x18U,
    AM_HAL_DC_LAYER_FORMAT_ABGR4444     = 0x0CU,
    AM_HAL_DC_LAYER_FORMAT_RGBA5551     = 0x01U,
    AM_HAL_DC_LAYER_FORMAT_BGRA5551     = 0x0FU,
    AM_HAL_DC_LAYER_FORMAT_ARGB1555     = 0x11U,
    AM_HAL_DC_LAYER_FORMAT_ABGR1555     = 0x1FU,

    // 24-bit format
    AM_HAL_DC_LAYER_FORMAT_RGB24        = 0x0BU,
    AM_HAL_DC_LAYER_FORMAT_BGR24        = 0x03U,

    // 32-bit formats
    AM_HAL_DC_LAYER_FORMAT_RGBA8888     = 0x0DU,
    AM_HAL_DC_LAYER_FORMAT_BGRA8888     = 0x06U,
    AM_HAL_DC_LAYER_FORMAT_ARGB8888     = 0x0EU,
    AM_HAL_DC_LAYER_FORMAT_ABGR8888     = 0x02U,
} am_hal_dc_layer_format_e;

//
//! Porter-Duff blend factors (source in bits[3:0], destination in bits[7:4]).
//! Blend modes below are (src_factor | (dst_factor << 4)).
//
#define AM_HAL_DC_BF_ZERO               (0x0U)  //!< Black / zero
#define AM_HAL_DC_BF_ONE                (0x1U)  //!< White / one
#define AM_HAL_DC_BF_SRCALPHA           (0x2U)  //!< Source alpha
#define AM_HAL_DC_BF_GLBALPHA           (0x3U)  //!< Global alpha
#define AM_HAL_DC_BF_SRCGBLALPHA        (0x4U)  //!< Source * global alpha
#define AM_HAL_DC_BF_INVSRCALPHA        (0x5U)  //!< 1 - source alpha
#define AM_HAL_DC_BF_INVGBLALPHA        (0x6U)  //!< 1 - global alpha
#define AM_HAL_DC_BF_INVSRCGBLALPHA     (0x7U)  //!< 1 - (source * global alpha)
#define AM_HAL_DC_BF_DSTALPHA           (0xAU)  //!< Destination alpha
#define AM_HAL_DC_BF_INVDSTALPHA        (0xBU)  //!< 1 - destination alpha

//
//! DC layer blend modes (Porter-Duff compositions of AM_HAL_DC_BF_*).
//
typedef enum
{
                            /*  source factor               destination factor */
    AM_HAL_DC_BLEND_SIMPLE   = (AM_HAL_DC_BF_SRCALPHA    | (AM_HAL_DC_BF_INVSRCALPHA << 4)), //!< Sa * Sa + Da * (1 - Sa)
    AM_HAL_DC_BLEND_CLEAR    = (AM_HAL_DC_BF_ZERO        | (AM_HAL_DC_BF_ZERO        << 4)), //!< 0
    AM_HAL_DC_BLEND_SRC      = (AM_HAL_DC_BF_ONE         | (AM_HAL_DC_BF_ZERO        << 4)), //!< Sa
    AM_HAL_DC_BLEND_SRC_OVER = (AM_HAL_DC_BF_ONE         | (AM_HAL_DC_BF_INVSRCALPHA << 4)), //!< Sa + Da * (1 - Sa)
    AM_HAL_DC_BLEND_DST_OVER = (AM_HAL_DC_BF_INVDSTALPHA | (AM_HAL_DC_BF_ONE         << 4)), //!< Sa * (1 - Da) + Da
    AM_HAL_DC_BLEND_SRC_IN   = (AM_HAL_DC_BF_DSTALPHA    | (AM_HAL_DC_BF_ZERO        << 4)), //!< Sa * Da
    AM_HAL_DC_BLEND_DST_IN   = (AM_HAL_DC_BF_ZERO        | (AM_HAL_DC_BF_SRCALPHA    << 4)), //!< Da * Sa
    AM_HAL_DC_BLEND_SRC_OUT  = (AM_HAL_DC_BF_INVDSTALPHA | (AM_HAL_DC_BF_ZERO        << 4)), //!< Sa * (1 - Da)
    AM_HAL_DC_BLEND_DST_OUT  = (AM_HAL_DC_BF_ZERO        | (AM_HAL_DC_BF_INVSRCALPHA << 4)), //!< Da * (1 - Sa)
    AM_HAL_DC_BLEND_SRC_ATOP = (AM_HAL_DC_BF_DSTALPHA    | (AM_HAL_DC_BF_INVSRCALPHA << 4)), //!< Sa * Da + Da * (1 - Sa)
    AM_HAL_DC_BLEND_DST_ATOP = (AM_HAL_DC_BF_INVDSTALPHA | (AM_HAL_DC_BF_SRCALPHA    << 4)), //!< Sa * (1 - Da) + Da * Sa
    AM_HAL_DC_BLEND_ADD      = (AM_HAL_DC_BF_ONE         | (AM_HAL_DC_BF_ONE         << 4)), //!< Sa + Da
    AM_HAL_DC_BLEND_XOR      = (AM_HAL_DC_BF_INVDSTALPHA | (AM_HAL_DC_BF_INVSRCALPHA << 4)), //!< Sa * (1 - Da) + Da * (1 - Sa)
} am_hal_dc_blend_mode_e;

//
//! Display Controller Interface Types.
//
typedef enum
{
    // Interfaces are directly supported by the DC hardware.
    AM_HAL_DC_IF_SPI4   = 0x1U << 0,    //!< Standard 4-wire SPI interface.
    AM_HAL_DC_IF_DSPI   = 0x1U << 1,    //!< Dual SPI interface.
    AM_HAL_DC_IF_QSPI   = 0x1U << 2,    //!< Quad SPI interface.
    AM_HAL_DC_IF_JDI    = 0x1U << 3,    //!< JDI/MiP (Memory-in-Pixel) interface.

    // DBI and DPI on Atomiq110 are only internal paths that feed into DSI host, are not routed to IO pads.
    AM_HAL_DC_IF_DPI    = 0x1U << 4,    //!< Display Pixel Interface (Parallel) to DSI host. Current DSI host supports DPI interface as input.
    AM_HAL_DC_IF_DBI    = 0x1U << 5,    //!< Display Bus Interface (8080) to DSI bridge. A bridge is required because the DSI host does not accept DBI natively.
    AM_HAL_DC_IF_MAX
} am_hal_dc_if_e;

//
//! CRM clock selection and dividers used by the DC module.
//
typedef struct
{
    // The module clock source.
    CRM_DISPPLLCRM_DISPPLLCLKSEL_Enum   ePllClkSel;         //!< CRM PLL/AXI clock selection.
    uint8_t                             ui8PllClkDiv;       //!< CRM PLL clock divider.

    // NemaDC clock dividers (computed by am_hal_dc_config()).
    uint8_t                             ui8Predivider;      //!< Input clock pre-divider (1..127).
    uint8_t                             ui8PrimaryDivider;  //!< Primary divider (JDI path; min 2).
    uint8_t                             ui8Ratio;             //!< Read-clock divider relative to write clock SPI.
} am_hal_crm_dc_clock_t;

//
// NemaDC type aliases (see nema_dc_*.h for field definitions).
//
typedef MiP_display_config_t        am_hal_dc_jdi_timing_t;   //!< JDI/MiP panel timing.
typedef nemadc_layer_t              am_hal_dc_layer_t;        //!< Compositing layer descriptor.
typedef nemadc_display_t            am_hal_dc_timing_t;       //!< Horizontal/vertical porch and resolution.

//
//! Configuration for QSPI, DSPI, and SPI interfaces.
//
typedef struct
{
    am_hal_dc_timing_t              sTiming;            //!< Timing for standard (non-JDI) interfaces.
    uint32_t                        ui32PolPha;         //!< SPI CPOL/CPHA and clock-swap/invert bits.
    bool                            bDDREnable;         //!< Enable DDR mode for QSPI.
    float                           fTargetClkWrite;    //!< Target write clock frequency in MHz.
    float                           fTargetClkRead;     //!< Target read clock frequency in MHz.
} am_hal_dc_xspi_cfg_t;

//
//! Configuration for DBI interfaces.
//
typedef struct
{
    am_hal_dc_timing_t              sTiming;            //!< Timing for standard (non-JDI) interfaces.
    float                           fTargetClkWRX;      //!< Target write clock frequency in MHz.
} am_hal_dc_dbi_cfg_t;

//
//! Configuration for DPI interface.
//
typedef struct
{
    am_hal_dc_timing_t              sTiming;            //!< Timing for standard (non-JDI) interfaces.
    float                           fTargetClkPClk;     //!< Target output clock frequency in MHz.
} am_hal_dc_dpi_cfg_t;

//
//! Configuration for JDI (MiP) only.
//
typedef struct
{
    am_hal_dc_jdi_timing_t          sJDITiming;         //!< Timing structure for JDI/MiP interface.
    float                           fTargetHCKBCKClk;   //!< Target HCK/BCK clock frequency in MHz.
    float                           fTargetVCKGCKFF;    //!< Target Fast forward VCK/GCK frequency in MHz.
} am_hal_dc_jdi_cfg_t;

//
//! Display Controller Configuration Structure.
//
typedef struct
{
    am_hal_dc_if_e                  eInterface;     //!< Selected display interface.
    bool                            bTEEnable;      //!< Enable Tearing Effect (TE) signal.
    union
    {
        uint32_t                    ui32DstFormat;  //!< Packed destination format (OR of AM_HAL_DC_FORMAT_* / IF_*).
        struct
        {
            uint32_t                ui32Format : 3; //!< Pixel format sub-field.
            uint32_t                ui32Option : 3; //!< Panel option sub-field (AM_HAL_DC_IF_OPTION*).
            uint32_t                ui32IFWidth : 3; //!< Bus width sub-field (AM_HAL_DC_IF_CFG_*).
            uint32_t                : 23;           //!< Reserved.
        } ui32DstFormat_b;
    } ;
    union
    {
        am_hal_dc_dbi_cfg_t         sDBICfg;        //!< Valid when eInterface == AM_HAL_DC_IF_DBI.
        am_hal_dc_dpi_cfg_t         sDPICfg;        //!< Valid when eInterface == AM_HAL_DC_IF_DPI.
        am_hal_dc_xspi_cfg_t        sSPICfg;        //!< Valid for SPI, DSPI, or QSPI.
        am_hal_dc_jdi_cfg_t         sJDICfg;        //!< Valid when eInterface == AM_HAL_DC_IF_JDI.
    };
} am_hal_dc_config_t;

//
//! Callback registration for TE or transfer completion.
//
typedef void (*am_hal_dc_callback_t)(void *pCallbackCtxt);
//
//! DC Control Request Types.
//
typedef enum
{
    // Clock and power
    AM_HAL_DC_REQ_SET_CLK,                      //!< Update input clock frequency (pArgs: am_hal_crm_dc_clock_t*; call before power-on).
    AM_HAL_DC_REQ_SET_CG,                       //!< Enable/disable clock gating (pArgs: bool*).

    // Tearing effect
    AM_HAL_DC_REQ_SET_TE_ENABLE,                //!< Enable TE signal, register TE callback (pArgs: am_hal_dc_callback_t*).
    AM_HAL_DC_REQ_SET_TE_DISABLE,               //!< Disable TE signal.

    AM_HAL_DC_REQ_SET_LAYER_GAMMA_LUT,          //!< Set layer gamma LUT (pArgs: am_hal_dc_gamma_lut_t*).
    AM_HAL_DC_REQ_GET_LAYER_GAMMA_LUT,          //!< Get layer gamma LUT (pArgs: am_hal_dc_gamma_lut_t*).
    AM_HAL_DC_REQ_SET_PALETTE,                  //!< Set palette entry (pArgs: am_hal_dc_palette_t*).
    AM_HAL_DC_REQ_GET_PALETTE,                  //!< Get palette entry (pArgs: am_hal_dc_palette_t*).

    AM_HAL_DC_REQ_GET_CRC_VALUE,                //!< Get CRC value (pArgs: uint32_t*).
    // Capability queries
    AM_HAL_DC_REQ_GET_AVAILABLE_LAYERS_CNT,     //!< Get supported layer count (pArgs: uint32_t*).
    AM_HAL_DC_REQ_GET_IP_VERSION,               //!< Get NemaDC IP version (pArgs: uint32_t*).
} am_hal_dc_request_e;

//
//! Per-channel gamma LUT entry for a compositing layer.
//
typedef struct
{
    int         i32Layer;   //!< Layer index (0 or 1).
    int         i32Index;   //!< LUT entry index.
    int         i32Color;   //!< Gamma-corrected color value (get/set).
} am_hal_dc_gamma_lut_t;

//
//! Indexed-color palette entry (LUT8 / AL44 formats).
//
typedef struct
{
    uint32_t     ui32Index; //!< Palette slot (0..255).
    uint32_t     ui32Color; //!< RGBA color value.
} am_hal_dc_palette_t;

//
//! DC Transfer Direction.
//
typedef enum
{
    AM_HAL_DC_WRITE,                    //!< Transmit data to display.
    AM_HAL_DC_READ                      //!< Receive data from display.
}
am_hal_dc_transfer_type_e;

//
//! DC transfer descriptor.
//!
//! Use the @c command member for register-oriented transfers (SPI/DBI/QSPI/DSPI).
//! Use the @c frame member for JDI partial-frame updates; for other interfaces,
//! frame transfers also use the @c command member fields.
//
typedef struct
{
    uint8_t                                     ui8Command;     //!< MIPI DCS command (see AM_HAL_DC_CMD_TYPE_*).
    union
    {
        struct
        {
            am_hal_dc_transfer_type_e           eDirection;     //!< Transfer direction.
            uint8_t                             *pui8TxBuf;     //!< Transmit payload (command transfers).
            uint32_t                            ui32TxLength;   //!< Transmit length in bytes.

            uint32_t                            *pui32RxBuf;    //!< Receive buffer (read commands).
            uint32_t                            ui32RxLength;   //!< Receive length in bytes.
        } command;

        struct
        {
            int32_t                             *pi32TxBuf;     //!< JDI partial-update region pairs [x0,y0,x1,y1,...].
            uint32_t                            ui32TxLength;   //!< Number of regions (max 16); each region is two int32 values.
        } frame;
    };
} am_hal_dc_transfer_t;

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Initialize the DC module.
//!
//! @param ui32Module   - Instance of the DC module (typically 0).
//! @param ppHandle     - Pointer to a handle to be populated with the DC state.
//!
//! This function must be called first to obtain a handle for subsequent DC
//! operations.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief Uninitialize the DC module.
//!
//! @param pHandle      - The DC state handle.
//!
//! Releases the handle reference in the sense of the HAL API; static module state
//! is torn down through am_hal_dc_power_control().
//!
//! @note The implementation does not heap-allocate the handle; it is module static
//! storage.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_uninitialize(void *pHandle);

//*****************************************************************************
//
//! @brief Control the power state of the DC module.
//!
//! @param pHandle      - The DC state handle.
//! @param ePowerState  - Desired power state (WAKE, NORMALSLEEP, DEEPSLEEP).
//! @param bRetainState - If true, hardware state is preserved (if supported).
//!
//! This function handles the power-up and power-down sequences for the DC,
//! including peripheral enabling, register restoration, and interrupt management.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_power_control(void *pHandle,
                                am_hal_sysctrl_power_state_e ePowerState,
                                bool bRetainState);

//*****************************************************************************
//
//! @brief Configure the DC module with specified parameters.
//!
//! @param pHandle      - The DC state handle.
//! @param pConfig      - Pointer to the am_hal_dc_config_t structure.
//!
//! This function performs the core configuration of the DC, including clock
//! calculation, interface-specific setup (SPI, QSPI, JDI, DPI), and timing
//! parameter application.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_config(void *pHandle, const am_hal_dc_config_t *pConfig);

//*****************************************************************************
//
//! @brief Perform a specific control operation on the DC.
//!
//! @param pHandle      - The DC state handle.
//! @param eRequest     - The control request type (am_hal_dc_request_e).
//! @param pArgs        - Pointer to request-specific arguments.
//!
//! Allows fine-grained control over hardware features like TE or clock gating.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_control(void *pHandle, am_hal_dc_request_e eRequest,
                           const void *pArgs);

//*****************************************************************************
//
//! @brief Initiate a non-blocking data transaction.
//!
//! @param pHandle      - The DC state handle.
//! @param pTransfer    - Pointer to the am_hal_dc_transfer_t descriptor.
//! @param pfnCallback  - Optional callback function to be called on completion.
//! @param pCallbackCtxt- Context pointer passed to the callback.
//!
//! Starts a non-blocking frame (bulk) transfer. Command transfers (panel
//! register writes/reads) must use am_hal_dc_blocking_transfer() instead.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_nonblocking_transfer(void *pHandle,
                                        am_hal_dc_transfer_t *pTransfer,
                                        void (*pfnCallback)(void*),
                                        void *pCallbackCtxt);

//*****************************************************************************
//
//! @brief Perform a blocking command/data stream transaction.
//!
//! @param pHandle      - The DC state handle.
//! @param pTransfer    - Pointer to the am_hal_dc_transfer_t descriptor.
//! @param ui32Timeoutus- Timeout in microseconds.
//!
//! Starts a blocking transfer and waits for completion or timeout.
//!   - SPI/DBI/QSPI/DSPI: use for both command and frame transfers.
//!   - DPI/JDI: frame transfers only (non-blocking API also available).
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_blocking_transfer(void *pHandle,
                                     am_hal_dc_transfer_t *pTransfer,
                                     uint32_t ui32Timeoutus);

//*****************************************************************************
//
//! @brief Enable specific DC interrupts.
//!
//! @param pHandle      - The DC state handle.
//! @param ui32IntMask  - Bitmask of interrupts to enable.
//!
//! Enable one interrupt source at a time so the service routine can tell which
//! event occurred.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_interrupt_enable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Disable specific DC interrupts.
//!
//! @param pHandle      - The DC state handle.
//! @param ui32IntMask  - Bitmask of interrupts to disable.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_interrupt_disable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Retrieve the current interrupt status.
//!
//! @param pHandle      - The DC state handle.
//! @param pui32Status  - Pointer to store the interrupt status mask.
//! @param bEnabledOnly - If true, only returns status of enabled interrupts.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_interrupt_status_get(void *pHandle, uint32_t *pui32Status,
                                        bool bEnabledOnly);

//*****************************************************************************
//
//! @brief Clear specific DC interrupts.
//!
//! @param pHandle      - The DC state handle.
//! @param ui32IntMask  - Bitmask of interrupts to clear.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_interrupt_clear(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief Process DC interrupts in an ISR context.
//!
//! @param pHandle      - The DC state handle.
//! @param ui32IntStatus- The interrupt status retrieved from hardware.
//!
//! Handles internal state updates and callback execution based on interrupts.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_interrupt_service(void *pHandle, uint32_t ui32IntStatus);

//*****************************************************************************
//
//! @brief Get the current hardware status of the DC.
//!
//! @param pHandle      - The DC state handle.
//! @param pui32Status  - Pointer to store the hardware status register value.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_status_get(void *pHandle, uint32_t *pui32Status);

//*****************************************************************************
//
//! @brief Configure DC layer.
//!
//! @param pHandle       - The DC state handle.
//! @param i32LayerIndex - The Layer index.
//! @param pLayer        - Pointer to the layer configuration (@ref am_hal_dc_layer_t).
//!
//! Call before frame transmission; otherwise only the background color is output.
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_set(void *pHandle, int32_t i32LayerIndex,
                             am_hal_dc_layer_t *pLayer);

//*****************************************************************************
//
//! @brief Configure DC physical layer.
//!
//! @param pHandle       - The DC state handle.
//! @param i32LayerIndex - The Layer index.
//! @param uiAddr        - Physical framebuffer address.
//!
//! Optional helper to update the layer framebuffer address after
//! am_hal_dc_layer_set().
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_addr_set(void *pHandle, int32_t i32LayerIndex,
                                  uintptr_t uiAddr);

//*****************************************************************************
//
//! @brief Calculate layer stride size
//!
//! @param pHandle      - The DC state handle.
//! @param ui32Format   - The layer color format.
//! @param i32Width     - The X resolution of the layer
//!
//! Calculate layer stride size in bytes.
//!
//! @return status      - stride size in bytes
//
//*****************************************************************************
uint32_t am_hal_dc_stride_size_get(void *pHandle, uint32_t ui32Format,
                                   int32_t i32Width);

//*****************************************************************************
//
//! @brief Set DC background color
//!
//! @param pHandle      - The DC state handle.
//! @param ui32Rgba     - The background color, MSB byte is Red, LSB byte is Alpha.
//!
//! Set the background color, the area not covered by the layer display this color.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_bgcolor_set(void *pHandle, uint32_t ui32Rgba);

//*****************************************************************************
//
//! @brief Enable DC layer
//!
//! @param pHandle       - The DC state handle.
//! @param i32LayerIndex - The Layer index.
//!
//! Call after am_hal_dc_layer_set() to enable the layer.
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_enable(void *pHandle, int32_t i32LayerIndex);

//*****************************************************************************
//
//! @brief Disable DC layer
//!
//! @param pHandle       - The DC state handle.
//! @param i32LayerIndex - The Layer index.
//!
//! Call after am_hal_dc_layer_set() to disable the layer.
//!
//! @return status       - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_disable(void *pHandle, int32_t i32LayerIndex);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_DC_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
