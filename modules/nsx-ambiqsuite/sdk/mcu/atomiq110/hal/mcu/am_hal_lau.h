//*****************************************************************************
//
//! @file am_hal_lau.h
//!
//! @brief Hardware Abstraction Layer for the GPU Framebuffer Linear Access Unit (LAU).
//!
//! @addtogroup lau LAU (Linear Access Unit)
//! @ingroup atomiq110_hal
//! @{
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

#ifndef AM_HAL_LAU_H
#define AM_HAL_LAU_H

#include "am_mcu_apollo.h"
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
//
// TODO: Use the register offsets from the Atomiq110 CMSIS header when available.
//
#define AM_HAL_LAU_BASEADDR     (0x40000000UL)
#define BIT(x)                  (1u << (x))
#define WRITE_REG(reg, value)   (*((volatile uint32_t *)(AM_HAL_LAU_BASEADDR + (reg))) = (value))
#define READ_REG(reg)           (*((volatile uint32_t *)(AM_HAL_LAU_BASEADDR + (reg))))

//
// Register offsets.
//
#define REG_CTRL                0x000
#define REG_STATUS              0x004
#define REG_IRQ_EN              0x008
#define REG_IRQ_STATUS          0x00C

#define REG_STRIDE              0x010
#define REG_SURFACE_PITCH       0x014
#define REG_PIX_FMT             0x018

#define REG_WIN_X               0x01C
#define REG_WIN_Y               0x020
#define REG_WIN_W               0x024
#define REG_WIN_H               0x028

#define REG_BURST_CFG           0x02C
#define REG_ARB_CREDIT          0x030
#define REG_LINE_IRQ_Y          0x034

#define REG_FB_WIN0_FB_BASE     0x038
#define REG_FB_WIN0_LIN_BASE    0x03C
#define REG_FB_WIN0_SIZE        0x040

#define REG_FB_WIN1_FB_BASE     0x044
#define REG_FB_WIN1_LIN_BASE    0x048
#define REG_FB_WIN1_SIZE        0x04C


//
// CTRL bits.
//
#define CTRL_EN                 BIT(0)
#define CTRL_SOFT_RST           BIT(1)
#define CTRL_DBL_BUF_EN         BIT(2)
#define CTRL_AUTO_SWAP          BIT(3)
#define CTRL_SW_BUF_SWAP        BIT(4)
#define CTRL_WC_FLUSH           BIT(5)
#define CTRL_MODE               BIT(6)
#define CTRL_CLK_MUX_SEL_SHIFT  (7)
#define CTRL_CLK_MUX_SEL_MASK   (0x3u << CTRL_CLK_MUX_SEL_SHIFT)

//
// STATUS bits.
//
#define STATUS_BUSY             BIT(0)
#define STATUS_DISP_ACTIVE      BIT(1)
#define STATUS_CUR_SCANLINE_LSB_SHIFT   8
#define STATUS_CUR_SCANLINE_LSB_MASK    (0xFFu << STATUS_CUR_SCANLINE_LSB_SHIFT)
#define STATUS_CUR_SCANLINE_MSB_SHIFT   16
#define STATUS_CUR_SCANLINE_MSB_MASK    (0x7u << STATUS_CUR_SCANLINE_MSB_SHIFT)
#define STATUS_ERR_SHIFT        28
#define STATUS_ERR_MASK         (0xFu << STATUS_ERR_SHIFT)

//
// IRQ bits.
//
#define IRQ_VSYNC               BIT(0)
#define IRQ_LINE                BIT(1)
#define IRQ_ERR                 BIT(2)

//
// PIX_FMT register values.
//
#define PIX_FMT_OPAQUE_8        0x00
#define PIX_FMT_L8              0x01
#define PIX_FMT_A8              0x02
#define PIX_FMT_CLUT8           0x03

#define PIX_FMT_OPAQUE_16       0x08
#define PIX_FMT_RGB565          0x09
#define PIX_FMT_RGBA5551        0x0A
#define PIX_FMT_ARGB1555        0x0B
#define PIX_FMT_RGBA4444        0x0C

#define PIX_FMT_OPAQUE_32       0x10
#define PIX_FMT_ARGB8888        0x11
#define PIX_FMT_ABGR8888        0x12
#define PIX_FMT_BGRA8888        0x13
#define PIX_FMT_RGBA8888        0x14

//
//! LAU maximum resolution
//
#define AM_HAL_LAU_MAX_H_RES    1280
#define AM_HAL_LAU_MAX_V_RES    720

//
//! LAU pixel format
//
typedef enum
{
    AM_HAL_LAU_PIX_FMT_OPAQUE_8  = PIX_FMT_OPAQUE_8,
    AM_HAL_LAU_PIX_FMT_L8        = PIX_FMT_L8,
    AM_HAL_LAU_PIX_FMT_A8        = PIX_FMT_A8,
    AM_HAL_LAU_PIX_FMT_CLUT8     = PIX_FMT_CLUT8,
    AM_HAL_LAU_PIX_FMT_OPAQUE_16 = PIX_FMT_OPAQUE_16,
    AM_HAL_LAU_PIX_FMT_RGB565    = PIX_FMT_RGB565,
    AM_HAL_LAU_PIX_FMT_RGBA5551  = PIX_FMT_RGBA5551,
    AM_HAL_LAU_PIX_FMT_ARGB1555  = PIX_FMT_ARGB1555,
    AM_HAL_LAU_PIX_FMT_RGBA4444  = PIX_FMT_RGBA4444,
    AM_HAL_LAU_PIX_FMT_OPAQUE_32 = PIX_FMT_OPAQUE_32,
    AM_HAL_LAU_PIX_FMT_ARGB8888  = PIX_FMT_ARGB8888,
    AM_HAL_LAU_PIX_FMT_ABGR8888  = PIX_FMT_ABGR8888,
    AM_HAL_LAU_PIX_FMT_BGRA8888  = PIX_FMT_BGRA8888,
    AM_HAL_LAU_PIX_FMT_RGBA8888  = PIX_FMT_RGBA8888,
} am_hal_lau_pix_fmt_e;

//
//! LAU DISP port mode
//
typedef enum
{
    AM_HAL_LAU_DISP_MODE_VIDEO = 0,
    AM_HAL_LAU_DISP_MODE_COMMAND = 1,
} am_hal_lau_disp_port_mode_e;

//
//! LAU Framebuffer
//
typedef enum
{
    AM_HAL_LAU_FRAMEBUFFER0 = 0,
    AM_HAL_LAU_FRAMEBUFFER1 = 1,
    AM_HAL_LAU_FRAMEBUFFER_NUM,
} am_hal_lau_framebuffer_e;

//
//! LAU Framebuffer Source Window Configuration
//
typedef struct
{
    uint32_t ui32X;                 //!< Source window start X in pixels.
    uint32_t ui32Y;                 //!< Source window start Y in pixels.
    uint32_t ui32W;                 //!< Source window width in pixels.
    uint32_t ui32H;                 //!< Source window height in pixels.
} am_hal_lau_window_cfg_t;

//
//! LAU Buffering Configuration
//
typedef struct
{
    bool bDoubleBufferEnable;       //!< Enable CTRL.DBL_BUF_EN.
    bool bAutoSwapEnable;           //!< Enable CTRL.AUTO_SWAP.
} am_hal_lau_buffering_cfg_t;

//
//! LAU DC Clock Source
//
typedef enum
{
    AM_HAL_LAU_DC_CLK_MUX_OFF    = 0,  //!< 00: tied low.
    AM_HAL_LAU_DC_CLK_MUX_CLKGEN = 1,  //!< 01: clkgen_nema_dc_clk.
    AM_HAL_LAU_DC_CLK_MUX_DSI_HS = 2,  //!< 10: dsi_hs_clk.
} am_hal_lau_dc_clk_mux_e;

//
//! LAU Framebuffer Configuration
//
typedef struct
{
    uint32_t ui32BaseAddr;          //!< Tiled framebuffer base address.
} am_hal_lau_framebuffer_cfg_t;

//
//! LAU surface configuration.
//!
//! Describes the source framebuffer surface.
//! The HAL uses this information to program LAU surface pitch and tile-row
//! stride registers.
//!
//! Note: LAU hardware also has a STRIDE register, but that register represents
//! tile-row stride, not line stride. The HAL performs the conversion
//! internally.
//
typedef struct
{
    am_hal_lau_pix_fmt_e ePixFmt;   //!< Pixel format of the framebuffer surface.
    uint32_t ui32WidthPixels;       //!< Valid surface width in pixels, excluding padding.
    uint32_t ui32HeightPixels;      //!< Valid surface height in pixels.

    bool bAutoStride;               //!< true: HAL computes ui32StrideBytes automatically.
                                    //!< false: caller provides ui32StrideBytes.

    uint32_t ui32StrideBytes;       //!< Line stride in bytes.
                                    //!< Valid only when bAutoStride is false.
                                    //!< Must be >= width * bytes_per_pixel and 32-byte aligned.
} am_hal_lau_surface_cfg_t;

//
//! LAU actual surface information.
//! TODO:  if the struct only contains the actual stride bytes,
//!        consider simplifying to just return the stride bytes instead of a struct.
//!        if maybe add more fields that inclue the width/height/bpp, tile stride, buffer size,
//!        alignment in the future, then keep the struct as is.
//! Reports the final framebuffer stride used by the HAL.
//! If bAutoStride is true, ui32StrideBytes is the HAL-computed stride.
//! If bAutoStride is false, ui32StrideBytes is the validated caller-provided stride.
//
typedef struct
{
    uint32_t ui32StrideBytes;       //!< Actual graphics line stride in bytes.
} am_hal_lau_surface_info_t;

//
//! LAU Configuration
//
typedef struct
{
    //! GPU framebuffer surface configuration.
    am_hal_lau_surface_cfg_t sSurface;

    //! Tiled framebuffer base addresses.
    //! sFrameBuffer[AM_HAL_LAU_FRAMEBUFFER0] maps to FB_WIN0_FB_BASE.
    //! sFrameBuffer[AM_HAL_LAU_FRAMEBUFFER1] maps to FB_WIN1_FB_BASE.
    am_hal_lau_framebuffer_cfg_t sFrameBuffer[AM_HAL_LAU_FRAMEBUFFER_NUM];

    //! Source within the GPU framebuffer surface.
    //! Defines the rectangular region of interest in pixel coordinates.
    //! Maps to WIN_X / WIN_Y / WIN_W / WIN_H registers.
    am_hal_lau_window_cfg_t sWindow;

    //! Single/double buffer and swap policy.
    am_hal_lau_buffering_cfg_t sBuffering;
} am_hal_lau_config_t;

//
//! LAU Status
//
typedef struct
{
    bool bBusy;                                  //!< LAU busy status.
    bool bDispActive;                            //!< LAU Display linear access in progress
    am_hal_lau_framebuffer_e eActiveFramebuffer; //!< LAU active framebuffer.
    uint32_t ui32CurScanline;                    //!< LAU current scanline.
    uint32_t ui32Error;                          //!< LAU error status.
} am_hal_lau_status_t;

//
//! LAU Request
//
typedef enum
{
    //
    // CTRL bits
    //
    // Soft reset the GPU scanout.
    //! pArgs N/A
    AM_HAL_LAU_REQ_SOFT_RESET,
    //! Pass bool pointer as pArgs.
    AM_HAL_LAU_REQ_SET_DOUBLE_BUFFER,
    //! pArgs N/A
    AM_HAL_LAU_REQ_SET_AUTO_SWAP,
    //! pArgs N/A
    AM_HAL_LAU_REQ_SET_SW_BUF_SWAP,
    //! pArgs N/A
    AM_HAL_LAU_REQ_SET_WC_FLUSH,
    //! Display port mode set.
    //! Pass am_hal_lau_disp_port_mode_e pointer as pArgs.
    AM_HAL_LAU_REQ_SET_DISP_MODE,

    //
    // software control
    //
    //! get the surface info
    //! Pass am_hal_lau_surface_info_t * as pArgs
    AM_HAL_LAU_REQ_GET_SURFACE_INFO,
    AM_HAL_LAU_REQ_MAX
} am_hal_lau_request_e;

//*****************************************************************************
//
// External function definitions
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief LAU initialization function
//!
//! @param ui32Module   - module instance.
//! @param ppHandle     - returns the handle for the module instance.
//!
//! This function accepts a module instance, allocates the interface and then
//! returns a handle to be used by the remaining interface functions.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief LAU uninitialization function
//!
//! @param pHandle      - the handle for the module instance.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_uninitialize(void *pHandle);

//*****************************************************************************
//
//! @brief LAU enable function
//!
//! @param pHandle      - handle for the interface.
//!
//! This function enables the LAU for operation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_enable(void *pHandle);

//*****************************************************************************
//
//! @brief LAU disable function
//!
//! @param pHandle      - handle for the interface.
//!
//! This function disables the LAU from operation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_disable(void *pHandle);

//*****************************************************************************
//
//! @brief LAU configuration function
//!
//! @param pHandle      - handle for the LAU.
//! @param pConfig      - pointer to the LAU specific configuration.
//!
//! This function configures the LAU for operation.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_configure(void *pHandle,
                                     const am_hal_lau_config_t *pConfig);

//*****************************************************************************
//
//! @brief LAU control function
//!
//! @param pHandle      - handle for the LAU.
//! @param eRequest     - device specific special request code.
//! @param pArgs        - pointer to the request specific arguments.
//!
//! This function allows advanced settings
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_control(void *pHandle,
                                   am_hal_lau_request_e eRequest,
                                   const void *pArgs);

//*****************************************************************************
//
//! @brief LAU power control function
//!
//! @param pHandle       - handle for the interface.
//! @param ePowerState   - the desired power state to move the peripheral to.
//! @param bRetainState  - flag (if true) to save/restore peripheral state upon
//!                       power state change.
//!
//! This function updates the peripheral to a given power state.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_power_control(void *pHandle,
                                         am_hal_sysctrl_power_state_e ePowerState,
                                         bool bRetainState);

//*****************************************************************************
//
//! @brief LAU status function
//!
//! @param pHandle      - handle for the interface.
//! @param psStatus     - pointer to an interface specific structure used to
//!                       return the status of the interface.
//!
//! This function returns the current status of the interface.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_status_get(void *pHandle, am_hal_lau_status_t *psStatus);

//*****************************************************************************
//
//! @brief LAU source window configuration function
//!
//! @param pHandle      - handle for the interface.
//! @param pWindow      - pointer to the LAU source window configuration.
//!
//! This function configures the source window of the LAU.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_window_configure(void *pHandle,
                                            const am_hal_lau_window_cfg_t *pWindow);

//*****************************************************************************
//
//! @brief LAU enable interrupts function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function enables the specific indicated interrupts.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_enable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief LAU disable interrupts function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function disables the specified interrupts.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_disable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief LAU get interrupt status
//!
//! @param pHandle        - handle for the interface.
//! @param pui32IntStatus - pointer to a uint32_t to return the interrupt status
//!
//! This function returns the interrupt status for the given peripheral.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_status_get(void *pHandle, uint32_t *pui32IntStatus);

//*****************************************************************************
//
//! @brief LAU interrupt clear
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function clears the interrupts for the given peripheral.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_clear(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief LAU interrupt service routine
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask indicating
//!                       interrupts to be serviced
//!
//! This function is designed to be called from within the user defined ISR
//! (am_lau_isr) in order to service the non-blocking, queued, or DMA processing
//! for a given module instance.
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_service(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief LAU CPU access window enable function
//!
//! @param pHandle      - handle for the interface.
//! @param eFrameBuffer - framebuffer index.
//! @param bEnable      - enable or disable the framebuffer.
//!
//! This function enables or disables the CPU access to the framebuffer.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_cpu_access_enable(void *pHandle,
                                             am_hal_lau_framebuffer_e eFrameBuffer,
                                             bool bEnable);

//*****************************************************************************
//
//! @brief LAU CPU access read function
//!
//! @param pHandle      - handle for the interface.
//! @param eFrameBuffer - framebuffer index.
//! @param ui32Offset   - offset in the framebuffer.
//! @param pui8Data     - pointer to the destination data buffer.
//! @param ui32NumBytes - number of bytes to read.
//!
//! This function reads data from the framebuffer.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_cpu_access_read(void *pHandle,
                                           am_hal_lau_framebuffer_e eFrameBuffer,
                                           uint32_t ui32Offset,
                                           uint8_t *pui8Data,
                                           uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief LAU CPU access write function
//!
//! @param pHandle      - handle for the interface.
//! @param eFrameBuffer - framebuffer index.
//! @param ui32Offset   - offset in the framebuffer.
//! @param pui8Data     - pointer to the source data buffer.
//! @param ui32NumBytes - number of bytes to write.
//!
//! This function writes data to the framebuffer.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_cpu_access_write(void *pHandle,
                                            am_hal_lau_framebuffer_e eFrameBuffer,
                                            uint32_t ui32Offset,
                                            const uint8_t *pui8Data,
                                            uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief LAU set the DC clock mux
//!
//! @param pHandle      - handle for the interface.
//! @param eClkMux      - DC clock mux.
//!
//! This function sets the DC clock mux.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_dc_clk_mux_set(void *pHandle,
                                          am_hal_lau_dc_clk_mux_e eClkMux);

//*****************************************************************************
//
//! @brief LAU get the DC clock mux
//!
//! @param pHandle      - handle for the interface.
//! @param peClkMux     - pointer to the DC clock mux.
//!
//! This function gets the DC clock mux.
//!
//! @return status      - generic or interface specific status.
//!
//*****************************************************************************
extern uint32_t am_hal_lau_dc_clk_mux_get(void *pHandle,
                                          am_hal_lau_dc_clk_mux_e *peClkMux);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_LAU_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
