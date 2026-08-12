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
// This is part of revision npu-drop-2026.07.09 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_DC_H
#define AM_HAL_DC_H

#include <stdint.h>
#include <stdbool.h>

#include "hal/am_hal_sysctrl.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "nema_dc_jdi.h"
#include "nema_dc_mipi.h"

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************

//! Frame end interrupt bits mask.
#define AM_HAL_DC_FRAME_END_INTERRUPT_BITS_MASK         (DC_INTERRUPT_FRAMEENDIRQEN_Msk | \
                                                         DC_INTERRUPT_IFFRAMEDONEIRQEN_Msk)

//! Number of DC compositing layers.
#define LAYERS_NUMBER                                   (2) //!< Number of DC compositing layers.

//! command identifiers (mapped from NemaDC).
#define AM_HAL_DC_CMD_TYPE_ENTER_IDLE_MODE              MIPI_enter_idle_mode
#define AM_HAL_DC_CMD_TYPE_ENTER_INVERT_MODE            MIPI_enter_invert_mode
#define AM_HAL_DC_CMD_TYPE_ENTER_NORMAL_MODE            MIPI_enter_normal_mode
#define AM_HAL_DC_CMD_TYPE_ENTER_PARITAL_MODE           MIPI_enter_partial_mode
#define AM_HAL_DC_CMD_TYPE_ENTER_SLEEP_MODE             MIPI_enter_sleep_mode
#define AM_HAL_DC_CMD_TYPE_EXIT_IDLE_MODE               MIPI_exit_idle_mode
#define AM_HAL_DC_CMD_TYPE_EXIT_INVERT_MODE             MIPI_exit_invert_mode
#define AM_HAL_DC_CMD_TYPE_EXIT_SLEEP_MODE              MIPI_exit_sleep_mode
#define AM_HAL_DC_CMD_TYPE_GET_3D_CONTROL               MIPI_get_3D_control
#define AM_HAL_DC_CMD_TYPE_GET_ADDRESS_MODE             MIPI_get_address_mode
#define AM_HAL_DC_CMD_TYPE_GET_BLUE_CHANNEL             MIPI_get_blue_channel
#define AM_HAL_DC_CMD_TYPE_DIAGNOSTIC_RESULT            MIPI_get_diagnostic_result
#define AM_HAL_DC_CMD_TYPE_DISPLAY_MODE                 MIPI_get_display_mode
#define AM_HAL_DC_CMD_TYPE_GET_GREEN_CHANNEL            MIPI_get_green_channel
#define AM_HAL_DC_CMD_TYPE_GET_PIXEL_FORMAT             MIPI_get_pixel_format
#define AM_HAL_DC_CMD_TYPE_GET_POWER_MODE               MIPI_get_power_mode
#define AM_HAL_DC_CMD_TYPE_GET_RED_CHANNEL              MIPI_get_red_channel
#define AM_HAL_DC_CMD_TYPE_GET_SCANLINE                 MIPI_get_scanline
#define AM_HAL_DC_CMD_TYPE_GET_SIGNAL_MODE              MIPI_get_signal_mode
#define AM_HAL_DC_CMD_TYPE_NOP                          MIPI_nop
#define AM_HAL_DC_CMD_TYPE_READ_DDB_CONTINUE            MIPI_read_DDB_continue
#define AM_HAL_DC_CMD_TYPE_READ_DDB_START               MIPI_read_DDB_start
#define AM_HAL_DC_CMD_TYPE_READ_MEMORY_CONTINUE         MIPI_read_memory_continue
#define AM_HAL_DC_CMD_TYPE_READ_MEMORY_START            MIPI_read_memory_start
#define AM_HAL_DC_CMD_TYPE_SET_3D_CONTROL               MIPI_set_3D_control
#define AM_HAL_DC_CMD_TYPE_SET_ADDRESS_MODE             MIPI_set_address_mode
#define AM_HAL_DC_CMD_TYPE_SET_COLUMN_ADDRESS           MIPI_set_column_address
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_OFF              MIPI_set_display_off
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_ON               MIPI_set_display_on
#define AM_HAL_DC_CMD_TYPE_SET_GAMMA_CURVE              MIPI_set_gamma_curve
#define AM_HAL_DC_CMD_TYPE_SET_PAGE_ADDRESS             MIPI_set_page_address
#define AM_HAL_DC_CMD_TYPE_SET_PARTIAL_COLUMNS          MIPI_set_partial_columns
#define AM_HAL_DC_CMD_TYPE_SET_PARTIAL_ROWS             MIPI_set_partial_rows
#define AM_HAL_DC_CMD_TYPE_SET_PIXEL_FORMAT             MIPI_set_pixel_format
#define AM_HAL_DC_CMD_TYPE_SET_SCROLL_AREA              MIPI_set_scroll_area
#define AM_HAL_DC_CMD_TYPE_SET_SCROLL_START             MIPI_set_scroll_start
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_OFF                 MIPI_set_tear_off
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_ON                  MIPI_set_tear_on
#define AM_HAL_DC_CMD_TYPE_SET_TEAR_SCANLINE            MIPI_set_tear_scanline
#define AM_HAL_DC_CMD_TYPE_SET_VSYNC_TIMING             MIPI_set_vsync_timing
#define AM_HAL_DC_CMD_TYPE_SET_SOFT_RESET               MIPI_soft_reset
#define AM_HAL_DC_CMD_TYPE_WRITE_LUT                    MIPI_write_LUT
#define AM_HAL_DC_CMD_TYPE_WRITE_MEMORY_CONTINUE        MIPI_write_memory_continue
#define AM_HAL_DC_CMD_TYPE_WRITE_MEMORY_START           MIPI_write_memory_start
#define AM_HAL_DC_CMD_TYPE_SNAPSHOT                     MIPI_snapshot
#define AM_HAL_DC_CMD_TYPE_SET_DISPLAY_BRIGHTNESS       0x51
#define AM_HAL_DC_CMD_TYPE_GET_DISPLAY_BRIGHTNESS       0x52

//! pixel output formats.
#define AM_HAL_DC_FORMAT_RGB111                         MIPI_DCS_RGB111
#define AM_HAL_DC_FORMAT_RGB332                         MIPI_DCS_RGB332
#define AM_HAL_DC_FORMAT_RGB444                         MIPI_DCS_RGB444
#define AM_HAL_DC_FORMAT_RGBBW                          MIPI_DCS_BW
#define AM_HAL_DC_FORMAT_RGB565                         MIPI_DCS_RGB565
#define AM_HAL_DC_FORMAT_RGB666                         MIPI_DCS_RGB666
#define AM_HAL_DC_FORMAT_RGB888                         MIPI_DCS_RGB888

//! DBI/SPI physical interface format selectors.
#define AM_HAL_DC_IF_CFG_SPI                            MIPICFG_PF_SPI
#define AM_HAL_DC_IF_CFG_DSPI                           MIPICFG_PF_DSPI
#define AM_HAL_DC_IF_CFG_QSPI                           MIPICFG_PF_QSPI
#define AM_HAL_DC_IF_CFG_DBI8                           MIPICFG_PF_DBI8
#define AM_HAL_DC_IF_CFG_DBI16                          MIPICFG_PF_DBI16

//! Additional DBI/SPI interface options.
#define AM_HAL_DC_IF_OPTION0                            MIPICFG_PF_OPT0
#define AM_HAL_DC_IF_OPTION1                            MIPICFG_PF_OPT1
#define AM_HAL_DC_IF_OPTION2                            MIPICFG_PF_OPT2
#define AM_HAL_DC_IF_OPTION3                            MIPICFG_PF_OPT3
#define AM_HAL_DC_IF_OPTION4                            MIPICFG_PF_OPT4

//*****************************************************************************
//
// Type definitions
//
//*****************************************************************************

//
//! Layer input color formats.
//
typedef enum
{
    // 8-bit formats
    AM_HAL_DC_LAYER_FORMAT_LUT8         = NEMADC_LUT8,
    AM_HAL_DC_LAYER_FORMAT_A8           = NEMADC_A8,
    AM_HAL_DC_LAYER_FORMAT_L8           = NEMADC_L8,
    AM_HAL_DC_LAYER_FORMAT_AL44         = NEMADC_AL44,
    AM_HAL_DC_LAYER_FORMAT_RGB332       = NEMADC_RGB332,
    AM_HAL_DC_LAYER_FORMAT_RGBA2222     = NEMADC_RGBA2222,
    AM_HAL_DC_LAYER_FORMAT_BGRA2222     = NEMADC_BGRA2222,
    AM_HAL_DC_LAYER_FORMAT_ARGB2222     = NEMADC_ARGB2222,
    AM_HAL_DC_LAYER_FORMAT_ABGR2222     = NEMADC_ABGR2222,

    // 16-bit formats
    AM_HAL_DC_LAYER_FORMAT_RGB565       = NEMADC_RGB565,
    AM_HAL_DC_LAYER_FORMAT_BGR565       = NEMADC_BGR565,
    AM_HAL_DC_LAYER_FORMAT_AL88         = NEMADC_AL88,
    AM_HAL_DC_LAYER_FORMAT_RGBA4444     = NEMADC_RGBA4444,
    AM_HAL_DC_LAYER_FORMAT_BGRA4444     = NEMADC_BGRA4444,
    AM_HAL_DC_LAYER_FORMAT_ARGB4444     = NEMADC_ARGB4444,
    AM_HAL_DC_LAYER_FORMAT_ABGR4444     = NEMADC_ABGR4444,
    AM_HAL_DC_LAYER_FORMAT_RGBA5551     = NEMADC_RGBA5551,
    AM_HAL_DC_LAYER_FORMAT_BGRA5551     = NEMADC_BGRA5551,
    AM_HAL_DC_LAYER_FORMAT_ARGB1555     = NEMADC_ARGB1555,
    AM_HAL_DC_LAYER_FORMAT_ABGR1555     = NEMADC_ABGR1555,

    // 24-bit format
    AM_HAL_DC_LAYER_FORMAT_RGB24        = NEMADC_RGB24,
    AM_HAL_DC_LAYER_FORMAT_BGR24        = NEMADC_BGR24,

    // 32-bit formats
    AM_HAL_DC_LAYER_FORMAT_RGBA8888     = NEMADC_RGBA8888,
    AM_HAL_DC_LAYER_FORMAT_BGRA8888     = NEMADC_BGRA8888,
    AM_HAL_DC_LAYER_FORMAT_ARGB8888     = NEMADC_ARGB8888,
    AM_HAL_DC_LAYER_FORMAT_ABGR8888     = NEMADC_ABGR8888,
} am_hal_dc_layer_format_e;

//
//! DC layer blend modes (Porter-Duff).
//
typedef enum
{
    AM_HAL_DC_BLEND_SIMPLE      = NEMADC_BL_SIMPLE,         // Sa * Sa + Da * (1 - Sa)
    AM_HAL_DC_BLEND_CLEAR       = NEMADC_BL_CLEAR,          // 0
    AM_HAL_DC_BLEND_SRC         = NEMADC_BL_SRC,            // Sa
    AM_HAL_DC_BLEND_SRC_OVER    = NEMADC_BL_SRC_OVER,       // Sa + Da * (1 - Sa)
    AM_HAL_DC_BLEND_DST_OVER    = NEMADC_BL_DST_OVER,       // Sa * (1 - Da) + Da
    AM_HAL_DC_BLEND_SRC_IN      = NEMADC_BL_SRC_IN,         // Sa * Da
    AM_HAL_DC_BLEND_DST_IN      = NEMADC_BL_DST_IN,         // Da * Sa
    AM_HAL_DC_BLEND_SRC_OUT     = NEMADC_BL_SRC_OUT,        // Sa * (1 - Da)
    AM_HAL_DC_BLEND_DST_OUT     = NEMADC_BL_DST_OUT,        // Da * (1 - Sa)
    AM_HAL_DC_BLEND_SRC_ATOP    = NEMADC_BL_SRC_ATOP,       // Sa * Da + Da * (1 - Sa)
    AM_HAL_DC_BLEND_DST_ATOP    = NEMADC_BL_DST_ATOP,       // Sa * (1 - Da) + Da * Sa
    AM_HAL_DC_BLEND_ADD         = NEMADC_BL_ADD,            // Sa + Da
    AM_HAL_DC_BLEND_XOR         = NEMADC_BL_XOR,            // Sa * (1 - Da) + Da * (1 - Sa)
} am_hal_dc_blend_mode_e;

//
//! Display Controller Interface Types.
//
typedef enum
{
    // Interfaces are directly supported by the DC hardware.
    AM_HAL_DC_IF_SPI    = 0x1U << 0,    //!< Standard 4-wire SPI interface.
    AM_HAL_DC_IF_DSPI   = 0x1U << 1,    //!< Dual SPI interface.
    AM_HAL_DC_IF_QSPI   = 0x1U << 2,    //!< Quad SPI interface.
    AM_HAL_DC_IF_JDI    = 0x1U << 3,    //!< JDI/MiP (Memory-in-Pixel) interface.

    // DBI and DPI on Atomiq110 are only internal paths that feed into DSI host, are not routed to IO pads.
    AM_HAL_DC_IF_DPI    = 0x1U << 4,    //!< Display Pixel Interface (Parallel) to DSI host. Current DSI host supports DPI interface as input.
    AM_HAL_DC_IF_DBI    = 0x1U << 5,    //!< Display Bus Interface (8080) to DSI bridge. A bridge is required because the DSI host does not accept DBI natively.
    AM_HAL_DC_IF_MAX
} am_hal_dc_if_e;

//
//! Display Controller PLL Clock source.
//
typedef enum
{
    AM_HAL_DC_PLL_HFRC_DIV2_DIV1,
    AM_HAL_DC_PLL_SYS_CLK3,
    AM_HAL_DC_PLL_DPHY_PLL_CLK
} am_hal_crm_dc_pll_clk_sel_e;

//
//! Display Controller AXI clock configuration.
//
typedef enum
{
    AM_HAL_DC_AXI_HFRC_DIV2_DIV2,
    AM_HAL_DC_AXI_HFRC_DIV2_DIV1
} am_hal_crm_dc_axi_clkcfg_e;

//
//! CRM clock selection and dividers used by the DC module.
//
typedef struct
{
    // The module clock source.
    am_hal_crm_dc_pll_clk_sel_e     ePllClkSel;
    uint8_t                         ui8PllClkDiv;
    am_hal_crm_dc_axi_clkcfg_e      eAxiClkCfg;

    // The dividers of DC
    uint8_t                         ui8Predivider;
    uint8_t                         ui8PrimaryDivider;
    // Write-read frequency ratio
    uint8_t                         ui8Ratio;
} am_hal_crm_dc_clock_t;

//
// Redefined structures.
//
typedef MiP_display_config_t        am_hal_dc_jdi_timing_t;
typedef nemadc_layer_t              am_hal_dc_layer_t;
typedef nemadc_display_t            am_hal_dc_timing_t;

//
//! Configuration for QSPI, DSPI, and SPI interfaces.
//
typedef struct
{
    am_hal_dc_timing_t              sTiming;            //!< Timing for standard (non-JDI) interfaces.
    uint32_t                        ui32PolPha;         //!< Polarity and Phase configuration
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
    uint32_t                        ui32PolPha;         //!< Polarity and Phase configuration
    float                           fTargetClkWRX;      //!< Target write clock frequency in MHz.
    float                           fTargetClkRDX;      //!< Target read clock frequency in MHz.
} am_hal_dc_dbi_cfg_t;

//
//! Configuration for DPI interface.
//
typedef struct
{
    am_hal_dc_timing_t              sTiming;            //!< Timing for standard (non-JDI) interfaces.
    uint32_t                        ui32PolPha;         //!< Polarity and Phase configuration
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
        uint32_t                    ui32DstFormat;  //!< Destination color format.
        struct
        {
            uint32_t                ui32Format : 3; //!< Format bits.
            uint32_t                ui32Option : 3; //!< Option bits.
            uint32_t                ui32IFWidth : 3; //!< Interface width.
            uint32_t                : 23;           //!< Reserved.
        } ui32DstFormat_b;
    } ;
    union
    {
        am_hal_dc_dbi_cfg_t         sDBICfg;        //!< DBI-specific configuration.
        am_hal_dc_dpi_cfg_t         sDPICfg;        //!< DPI-specific configuration.
        am_hal_dc_xspi_cfg_t        sSPICfg;        //!< SPI/DSPI/QSPI configuration.
        am_hal_dc_jdi_cfg_t         sJDICfg;        //!< JDI (MiP) configuration.
    };
} am_hal_dc_config_t;

//
//! Callback registration for TE or transfer completion.
//
typedef struct
{
    am_hal_dc_callback_t                pfnCallback;    //!< Callback function pointer.
    void                                *pCallbackCtxt; //!< Opaque context passed to the callback.
} am_hal_dc_cb_t;

//
//! DC Control Request Types.
//
typedef enum
{
    // Pass am_hal_crm_dc_clock_t as pArgs
    AM_HAL_DC_REQ_SET_CLK,                      //!< Update input clock frequency.
    // Pass bool as pArgs
    AM_HAL_DC_REQ_SET_CG,                       //!< Enable/disable clock gating.
    // Pass am_hal_dc_cb_t as pArgs
    AM_HAL_DC_REQ_SET_TE_ENABLE,                //!< Enable TE signal, register TE callback.
    // No pArgs
    AM_HAL_DC_REQ_SET_TE_DISABLE,               //!< Disable TE signal.
    // Pass int as pArgs
    AM_HAL_DC_REQ_SET_MODE,                     //!< Set mode, then write 1 to play.
    // Pass am_hal_dc_gamma_lut_t as the pArgs.
    AM_HAL_DC_REQ_SET_LAYER_GAMMA_LUT,          //!< Set layer gamma LUT.
    // Pass am_hal_dc_gamma_lut_t as the pArgs.
    AM_HAL_DC_REQ_GET_LAYER_GAMMA_LUT,          //!< Get layer gamma LUT.
    // Pass am_hal_dc_palette_t as the pArgs.
    AM_HAL_DC_REQ_SET_PALETTE,                  //!< Set palette.
    // Pass am_hal_dc_palette_t as the pArgs
    AM_HAL_DC_REQ_GET_PALETTE,                  //!< Get palette.
    // No pArgs
    AM_HAL_DC_REQ_GET_AVAILABLE_LAYERS_CNT,     //!< Get supported layers number.
    // No pArgs
    AM_HAL_DC_REQ_GET_IP_VERSION,               //!< Get hardware version.
} am_hal_dc_request_e;

//
// Layer Gamma LUT
//
typedef struct
{
    int         i32Layer;
    int         i32Index;
    int         i32Color;
} am_hal_dc_gamma_lut_t;

//
// Palette table
//
typedef struct
{
    uint32_t     ui32Index;
    uint32_t     ui32Color;
} am_hal_dc_palette_t;

//
//! DC Transfer Direction.
//
typedef enum
{
    AM_HAL_DC_WRITE,                    //!< Transmit data to display.
    AM_HAL_DC_READ                      //!< Receive data from display.
} am_hal_transfer_type_e;

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
            am_hal_transfer_type_e              eDirection;     //!< Transfer direction.
            uint8_t                             *pui8TxBuf;     //!< Transmit payload (command transfers).
            uint32_t                            ui32TxLength;   //!< Transmit length in bytes.

            uint32_t                            *pui32RxBuf;    //!< Receive buffer (read commands).
            uint32_t                            ui32RxLength;   //!< Receive length in bytes.
        } command;

        struct
        {
            int32_t                             *pi32TxBuf;     //!< JDI region table (up to 16 regions).
            uint32_t                            ui32TxLength;   //!< Number of JDI regions in @c pi32TxBuf.
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
//! Starts a non-blocking frame (bulk) transfer. Use
//! am_hal_dc_blocking_transfer() for command transfers.
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
//! @param ui32TimeoutMs- Timeout in milliseconds.
//!
//! Starts a blocking transfer and waits for completion or timeout. Use this API
//! for command transfers; frame transfers may be blocking or non-blocking.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_blocking_transfer(void *pHandle,
                                     am_hal_dc_transfer_t *pTransfer,
                                     uint32_t ui32TimeoutMs);

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
//! @param pHandle      - The DC state handle.
//! @param layerIndex   - The Layer index.
//! @param layer        - Pointer to the layer configuration (@ref am_hal_dc_layer_t).
//!
//! Call before frame transmission; otherwise only the background color is output.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_set(void *pHandle, int layerIndex, am_hal_dc_layer_t *layer);

//*****************************************************************************
//
//! @brief Configure DC physical layer.
//!
//! @param pHandle      - The DC state handle.
//! @param layerIndex   - The Layer index.
//! @param addr         - Physical framebuffer address.
//!
//! Optional helper to update the layer framebuffer address after
//! am_hal_dc_layer_set().
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_addr_set(void *pHandle, int layerIndex, uintptr_t addr);

//*****************************************************************************
//
//! @brief Calculate layer stride size
//!
//! @param pHandle      - The DC state handle.
//! @param format       - The layer color format.
//! @param width        - The X resolution of the layer
//!
//! Calculate layer stride size in bytes.
//!
//! @return status      - stride size in bytes
//
//*****************************************************************************
uint32_t am_hal_dc_stride_size_get(void *pHandle, uint32_t format, int width);

//*****************************************************************************
//
//! @brief Set DC background color
//!
//! @param pHandle      - The DC state handle.
//! @param rgba         - The background color, MSB byte is Red, LSB byte is Alpha.
//!
//! Set the background color, the area not covered by the layer display this color.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_bgcolor_set(void *pHandle, uint32_t rgba);

//*****************************************************************************
//
//! @brief Enable DC layer
//!
//! @param pHandle      - The DC state handle.
//! @param layerIndex   - The Layer index.
//!
//! Call after am_hal_dc_layer_set() to enable the layer.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_enable(void *pHandle, int layerIndex);

//*****************************************************************************
//
//! @brief Disable DC layer
//!
//! @param pHandle      - The DC state handle.
//! @param layerIndex   - The Layer index.
//!
//! Call after am_hal_dc_layer_set() to disable the layer.
//!
//! @return status      - AM_HAL_STATUS_SUCCESS on success.
//
//*****************************************************************************
uint32_t am_hal_dc_layer_disable(void *pHandle, int layerIndex);

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
