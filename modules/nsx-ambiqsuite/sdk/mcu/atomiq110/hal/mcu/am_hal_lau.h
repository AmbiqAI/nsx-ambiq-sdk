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
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
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
//! CMSIS-style macro for handling a variable LAU module number.
//
//*****************************************************************************
#define LAUn(n) ((LAU_Type*)(LAU_BASE + (n * (LAU_BASE - LAU_BASE))))

//*****************************************************************************
//
// Macro definitions
//
//*****************************************************************************
//
//! LAU maximum resolution
//
#define AM_HAL_LAU_MAX_H_RES    1280
#define AM_HAL_LAU_MAX_V_RES    720

//! LAU CPU aperture address range (guide section 2).
//! The whole 0x2500_0000-0x27FF_FFFF region must be mapped non-cacheable.
#define AM_HAL_LAU_CPU_APERTURE_BASE        0x25000000u
#define AM_HAL_LAU_CPU_APERTURE_LIMIT       0x27FFFFFFu
#define AM_HAL_LAU_FB_APERTURE_BASE         AM_HAL_LAU_CPU_APERTURE_BASE

#define AM_HAL_LAU_CSR_APERTURE_BASE        LAU_BASE     // 0x27FF0000 in the Atomiq110.h
#define AM_HAL_LAU_FB_APERTURE_LIMIT        (LAU_BASE -1)   // 0x27FF0000 - 1
#define AM_HAL_LAU_CSR_APERTURE_LIMIT       AM_HAL_LAU_CPU_APERTURE_LIMIT
//
//! LAU default linear base addresses for framebuffer windows.
//!
//! The LAU presents a rectangular window of the GPU surface to the CPU as
//! a dense linear aperture at FB_WINx_LIN_BASE.
//!
//! The default linear base addresses are 0x25000000 and 0x26000000.
//
#define AM_HAL_LAU_DEFAULT_FBWIN0_LIN_BASE    0x25000000u
#define AM_HAL_LAU_DEFAULT_FBWIN1_LIN_BASE    0x26000000u

//!
//! The maximum size of the LAU Framebuffer window.
//! @note The maximum size is 1280x720x4 (4 bytes per pixel).
#define AM_HAL_LAU_FBWIN_MAX_SIZE        0x00384000U

//*****************************************************************************
//
//! LAU tiled memory map
//!
//! The GPU writes each surface into its backing buffer (FB_WINx_FB_BASE) as
//! 8 x 8-pixel tiles. The LAU shows a rectangular window of that surface to
//! the CPU as a dense linear aperture (FB_WINx_LIN_BASE).
//!
//! Units used below:
//!   bpp            bytes per pixel (1, 2 or 4) - bytes, never bits
//!   width, height  GPU surface size in pixels
//!   win_x .. win_h WIN_X / WIN_Y / WIN_W / WIN_H, window in surface pixels
//!   tile_x, tile_y surface tile coordinates (not window-relative)
//!   offset         byte offset from FB_WINx_LIN_BASE
//
//*****************************************************************************

//! @brief Tile edge in pixels (tiles are 8 x 8).
#define AM_HAL_LAU_TILE_DIM     8u
//! @brief Pixels per tile: 8 * 8 = 64.
#define AM_HAL_LAU_TILE_PIXELS  (AM_HAL_LAU_TILE_DIM * AM_HAL_LAU_TILE_DIM)
//! @brief Bytes per pixel from PIX_FMT[4:3]: 00 -> 1, 01 -> 2, 10 -> 4.
//!        11 is reserved (CPU accesses return SLVERR); this macro returns 8.
#define AM_HAL_LAU_BYTES_PER_PIXEL(ePixFmt)                              \
    (1u << ((((uint32_t)(ePixFmt)) >> 3) & 0x3u))

//*****************************************************************************
//
//! 1) Surface tile grid (tile_x, tile_y); each cell is 8 x 8 pixels:
//!
//!      px x: 0       8       16          8*(TILES_X-1)
//!            +-------+-------+-- ... --+-------+  y = 0
//!            | (0,0) | (1,0) |         |       |
//!            +-------+-------+-- ... --+-------+  y = 8
//!            | (0,1) | (1,1) |         |       |
//!            +-------+-------+-- ... --+-------+  y = 16
//!            :       :       :         :       :
//!            +-------+-------+-- ... --+-------+  y = 8*TILES_Y
//!
//!      Pass the surface size here, not WIN_W / WIN_H.
//
//*****************************************************************************

//! @brief Tiles across the surface: ceil(width / 8).
#define AM_HAL_LAU_TILES_X(width)                                        \
    ((((uint32_t)(width))  + AM_HAL_LAU_TILE_DIM - 1u) / AM_HAL_LAU_TILE_DIM)
//! @brief Tiles down the surface: ceil(height / 8).
#define AM_HAL_LAU_TILES_Y(height)                                       \
    ((((uint32_t)(height)) + AM_HAL_LAU_TILE_DIM - 1u) / AM_HAL_LAU_TILE_DIM)

//*****************************************************************************
//
//! 2) Backing buffer layout (byte offsets from FB_WINx_FB_BASE), n = TILES_X:
//!
//!      FB_BASE
//!      |<- TILE_SIZE ->|<- TILE_SIZE ->| ... |<- TILE_SIZE ->|
//!      +---------------+---------------+ ... +---------------+
//!      |  tile (0,0)   |  tile (1,0)   |     | tile (n-1,0)  |  tile_y = 0
//!      +---------------+---------------+ ... +---------------+
//!      |<--------- TILE_STRIDE (= SURFACE_PITCH * 8) -------->|
//!      |               = n * TILE_SIZE (+ pitch pad)         |
//!      +---------------+---------------+ ... +---------------+
//!      |  tile (0,1)   |  tile (1,1)   |     | tile (n-1,1)  |  tile_y = 1
//!      +---------------+---------------+ ... +---------------+
//!      :               :               :     :               :
//!      +---------------+---------------+ ... +---------------+
//!                                          FB_BASE + TILED_SURFACE_SIZE
//!
//!      Conceptual (one scanline pitch vs one tile-row stride):
//!
//!      |<-------------- SURFACE_PITCH --------------->|  1 scanline
//!      |<-------------- SURFACE_PITCH --------------->|  ...
//!      |<-------------- SURFACE_PITCH --------------->|  x TILE_DIM (=8)
//!      |<======= TILE_STRIDE = SURFACE_PITCH * 8 ====>|  1 tile row
//!
//!      n             = TILES_X
//!      TILE_SIZE     = AM_HAL_LAU_TILE_SIZE_BYTES(bpp)  = 64 * bpp
//!                      (also required FB_WINx_FB_BASE alignment)
//!      SURFACE_PITCH = AM_HAL_LAU_SURFACE_PITCH_ALIGN(width, bpp)
//!                    = ceil(width * bpp, 32)   // or a larger padded pitch
//!      TILE_STRIDE   = AM_HAL_LAU_TILE_STRIDE(SURFACE_PITCH)
//!                    = SURFACE_PITCH * TILE_DIM  // * 8 = tile height (rows)
//!                    = n * TILE_SIZE             // when pitch packs the row
//!                      (effective STRIDE when STRIDE = 0; not 8 * TILE_SIZE)
//!      size          = AM_HAL_LAU_TILED_SURFACE_SIZE(height, SURFACE_PITCH)
//!                    = TILE_STRIDE * TILES_Y
//!
//!      tile_base(tile_x, tile_y) = FB_BASE + tile_y * TILE_STRIDE
//!                                          + tile_x * TILE_SIZE
//!
//!      If STRIDE is programmed explicitly (non-zero), use STRIDE in place of
//!      TILE_STRIDE. Pixel order inside a tile is the GPU swizzle; use the
//!      swizzle helper for raw pixel addresses, not this diagram.
//!
//! @note Programming Guide Sec. 3: STRIDE = 0 is derived from SURFACE_PITCH.
//!       This HAL uses SURFACE_PITCH * TILE_DIM for that derived value.
//
//*****************************************************************************
//! @brief Bytes in one tile: 64 * bpp. Also the FB_WINx_FB_BASE alignment.
#define AM_HAL_LAU_TILE_SIZE_BYTES(bytes_per_pixel)                      \
    (AM_HAL_LAU_TILE_PIXELS * (uint32_t)(bytes_per_pixel))
//! @brief Minimum SURFACE_PITCH: round_up(width * bpp, 32).
//!        Must match the GPU surface pitch; use the GPU's value if larger.
//!        Fill @c ui32PitchBytes with this before @c am_hal_lau_configure().
#define AM_HAL_LAU_SURFACE_PITCH_ALIGN(width, bytes_per_pixel)           \
    (((uint32_t)(width) * (uint32_t)(bytes_per_pixel) + 31u) & ~31u)
//! @brief Bytes from tile (x, y) to tile (x, y + 1): SURFACE_PITCH * 8.
//!        Effective STRIDE when STRIDE = 0 (see section note above).
#define AM_HAL_LAU_TILE_STRIDE(surface_pitch_bytes)                      \
    ((uint32_t)(surface_pitch_bytes) * AM_HAL_LAU_TILE_DIM)
//! @brief Bytes of the whole tiled buffer: TILE_STRIDE * TILES_Y(height).
//!        height is the full surface height (from y = 0).
#define AM_HAL_LAU_TILED_SURFACE_SIZE(height, surface_pitch_bytes)       \
    (AM_HAL_LAU_TILE_STRIDE(surface_pitch_bytes) * AM_HAL_LAU_TILES_Y(height))

//*****************************************************************************
//
//! 3) CPU window over the surface (WIN_X, WIN_Y, WIN_W, WIN_H). The window
//!    origin need not be tile-aligned. Example: WIN_X = 4, WIN_Y = 4,
//!    WIN_W = 16, WIN_H = 8:
//!
//!      px x: 0   4   8       16  20  24
//!            +-------+-------+-------+  y = 0
//!            |       |       |       |
//!            |   +===|=======|===+   |  y = 4    WIN_TILES_X(4, 16) = 3
//!            |   |   |       |   |   |
//!            +---|---+-------+---|---+  y = 8    WIN_TILES_Y(4, 8)  = 2
//!            |   |   |       |   |   |
//!            |   +===|=======|===+   |  y = 12
//!            |       |       |       |
//!            +-------+-------+-------+  y = 16
//!
//!      WIN_TILES_X = ceil((win_x + win_w) / 8) - floor(win_x / 8)
//!      WIN_TILES_Y = ceil((win_y + win_h) / 8) - floor(win_y / 8)
//!      WIN_TILED_OFFSET = floor(win_y / 8) * tile_stride
//!      WIN_TILED_SIZE   = WIN_TILES_Y * tile_stride
//!        (full surface width - superset of the window; for cache / raw checks)
//!      tile_stride = effective STRIDE (AM_HAL_LAU_TILE_STRIDE when STRIDE=0)
//
//*****************************************************************************
//! @brief Tile columns the window touches:
//!        ceil((win_x + win_w) / 8) - floor(win_x / 8).
#define AM_HAL_LAU_WIN_TILES_X(win_x, win_w)                             \
    ((((uint32_t)(win_x) + (uint32_t)(win_w) + AM_HAL_LAU_TILE_DIM - 1u)  \
      / AM_HAL_LAU_TILE_DIM) -                                           \
     ((uint32_t)(win_x) / AM_HAL_LAU_TILE_DIM))
//! @brief Tile rows the window touches:
//!        ceil((win_y + win_h) / 8) - floor(win_y / 8).
#define AM_HAL_LAU_WIN_TILES_Y(win_y, win_h)                             \
    ((((uint32_t)(win_y) + (uint32_t)(win_h) + AM_HAL_LAU_TILE_DIM - 1u)  \
      / AM_HAL_LAU_TILE_DIM) -                                           \
     ((uint32_t)(win_y) / AM_HAL_LAU_TILE_DIM))
//! @brief FB_BASE offset of the first tile row the window touches:
//!        floor(win_y / 8) * tile_stride.
#define AM_HAL_LAU_WIN_TILED_OFFSET(win_y, tile_stride)                  \
    (((uint32_t)(win_y) / AM_HAL_LAU_TILE_DIM) * (uint32_t)(tile_stride))
//! @brief Bytes of the full-width tile rows the window touches (superset of
//!        the window): WIN_TILES_Y * tile_stride. For cache / raw checks.
#define AM_HAL_LAU_WIN_TILED_SIZE(win_y, win_h, tile_stride)             \
    (AM_HAL_LAU_WIN_TILES_Y(win_y, win_h) * (uint32_t)(tile_stride))

//*****************************************************************************
//
//! 4) Linear aperture (byte offsets from FB_WINx_LIN_BASE), dense,
//!    pitch = WIN_W * bpp:
//!
//!      LIN_BASE (8-byte aligned)
//!      +-------------------- WIN_W * bpp --------------------+
//!      | window row 0 (surface y = WIN_Y)                    |
//!      | window row 1 (surface y = WIN_Y + 1)                |
//!      : ...                                                 :
//!      | window row WIN_H - 1                                |
//!      +-----------------------------------------------------+
//!                                    LIN_BASE + FB_WINx_SIZE
//!
//!      FB_WINx_SIZE = AM_HAL_LAU_FBWIN_SIZE(WIN_W, WIN_H, bpp)
//!                   = WIN_W * WIN_H * bpp  (program this exact value)
//!      offset(x, y) = AM_HAL_LAU_LINEAR_OFFSET(x, y, WIN_W * bpp, bpp)
//!                   = y * (WIN_W * bpp) + x * bpp
//!      LINEAR_OFFSET_TILE(tx, ty, ...) -> LIN offset of tile top-left
//!        (requires tx*8 >= win_x and ty*8 >= win_y)
//!      LINEAR_OFFSET_TO_TILE_X/Y -> surface tile index for a LIN offset
//!
//! Example: 320 x 320 RGB565 (bpp = 2), SURFACE_PITCH = 640 (32-byte aligned),
//! window = full surface:
//!      TILES_X = TILES_Y = 40, TILE_SIZE = 128, TILE_STRIDE = 5120
//!      tile (1,0) -> FB_BASE + 0x80,   linear offset 0x0010
//!      tile (0,1) -> FB_BASE + 0x1400, linear offset 0x1400
//!      TILED_SURFACE_SIZE = FBWIN_SIZE = 204800
//!      (equal only for full-surface window with no pitch padding)
//
//*****************************************************************************
//! @brief FB_WINx_SIZE register value: win_w * win_h * bpp.
#define AM_HAL_LAU_FBWIN_SIZE(win_w, win_h, bytes_per_pixel)             \
    ((uint32_t)(win_w) * (uint32_t)(win_h) * (uint32_t)(bytes_per_pixel))
//! @brief Aperture offset of window pixel (x, y): y * win_pitch + x * bpp.
//!        win_pitch_bytes = WIN_W * bpp, not SURFACE_PITCH.
#define AM_HAL_LAU_LINEAR_OFFSET(x, y, win_pitch_bytes, bytes_per_pixel) \
    ((uint32_t)(y) * (uint32_t)(win_pitch_bytes) +                       \
     (uint32_t)(x) * (uint32_t)(bytes_per_pixel))
//! @brief Aperture offset of the top-left pixel of surface tile
//!        (tile_x, tile_y). Requires tile_x * 8 >= win_x and
//!        tile_y * 8 >= win_y (top-left pixel inside the window).
#define AM_HAL_LAU_LINEAR_OFFSET_TILE(tile_x, tile_y, win_x, win_y,      \
                                      win_pitch_bytes, bytes_per_pixel)  \
    AM_HAL_LAU_LINEAR_OFFSET(                                            \
        (uint32_t)(tile_x) * AM_HAL_LAU_TILE_DIM - (uint32_t)(win_x),    \
        (uint32_t)(tile_y) * AM_HAL_LAU_TILE_DIM - (uint32_t)(win_y),    \
        (win_pitch_bytes), (bytes_per_pixel))
//! @brief Surface tile_x containing aperture offset: (win_x + x) / 8,
//!        x = (offset / bpp) % win_w. win_w is in pixels.
#define AM_HAL_LAU_LINEAR_OFFSET_TO_TILE_X(offset, win_x, win_w, bytes_per_pixel) \
    (((uint32_t)(win_x) +                                                \
      ((uint32_t)(offset) / (uint32_t)(bytes_per_pixel)) % (uint32_t)(win_w)) \
     / AM_HAL_LAU_TILE_DIM)
//! @brief Surface tile_y containing aperture offset: (win_y + y) / 8,
//!        y = (offset / bpp) / win_w. win_w is in pixels.
#define AM_HAL_LAU_LINEAR_OFFSET_TO_TILE_Y(offset, win_y, win_w, bytes_per_pixel) \
    (((uint32_t)(win_y) +                                                \
      ((uint32_t)(offset) / (uint32_t)(bytes_per_pixel)) / (uint32_t)(win_w)) \
     / AM_HAL_LAU_TILE_DIM)

//
//! The default LAU Framebuffer window linear base address(hardware recommended).
//
#define AM_HAL_LAU_FBWIN_DEFAULT_LIN_BASE(win)                                 \
                 ((uint32_t)((win) == 0 ? AM_HAL_LAU_DEFAULT_FBWIN0_LIN_BASE : \
                                          AM_HAL_LAU_DEFAULT_FBWIN1_LIN_BASE))

//
//! @brief LAU linear base address for DC layer configuration.
//!
//! The display controller (NemaDC) must always use this address as its layer
//! base in both single-buffer and double-buffer modes.
//!
//! Although two linear windows exist (FB_WIN0_LIN_BASE / FB_WIN1_LIN_BASE),
//! all DISP-port reads share a single internal scanline buffer inside the LAU.
//! The active data source is determined solely by the LAU @c buf_sel field
//! (see @c STATUS.ACTIVE_BUF), not by which linear base address DC reads from.
//! Switching DC's layer base between WIN0 and WIN1 linear addresses has no
//! effect on buffer selection and must not be done.
//!
//! - Single-buffer: @c buf_sel is fixed at 0; DC always reads @c FB_WIN0_FB_BASE.
//! - Double-buffer: LAU swaps the underlying tiled framebuffer
//!   (@c FB_WIN0_FB_BASE <-> @c FB_WIN1_FB_BASE) on each VSYNC via
//!   @c AUTO_SWAP or @c SW_BUF_SWAP, while this address remains constant.
//!
//! @note DC layer configuration example:
//! @code
//!     nemadc_layer_t layer = {0};
//!     layer.baseaddr_phys = (uint32_t)AM_HAL_LAU_DC_LAYER_ADDR;
//!     layer.baseaddr_virt = (void *)AM_HAL_LAU_DC_LAYER_ADDR;
//! @endcode
//
#define AM_HAL_LAU_DC_LAYER_ADDR    ((uint32_t*)(AM_HAL_LAU_DEFAULT_FBWIN0_LIN_BASE))

//
// LAU line Y default value
//
#define AM_HAL_LAU_LINE_IRQ_Y_DEFAULT       0xFFF8u

// CARSW-955 : STATUS.ERRFLAGS do not define in the RTL register description.
#define LAU_STATUS_ERRFLAGS_Pos           (28UL)                    /*!< ERRFLAGS (Bit 28)                                     */
#define LAU_STATUS_ERRFLAGS_Msk           (0xf0000000UL)
//
//! LAU status error flags (STATUS.ERRFLAGS[31:28], v2.62).
//! Bit 28 (ERR[0]): AXI MEM error
//! Bit 29 (ERR[1]): Soft reset while DISP responses owed
//! Bit 30 (ERR[2]): Sustained underflow / port stall; in command mode this
//! typically means the DC read stepped back into an already-consumed tile row.
//! Recovery is a soft reset (guide sections 6.1 and 11).
//! Bit 31 (ERR[3]): Invalid config / pair-parity reject
//! Reported unshifted in am_hal_lau_status_t.ui32Error.
//! Only ERR[0] raises the ERR interrupt (guide section 8).
//!
#define AM_HAL_LAU_STATUS_ERROR_AXI_MEM_ERROR               (1 << 0)
#define AM_HAL_LAU_STATUS_ERROR_SOFT_RST_WHILE_DISP_OWED    (1 << 1)
#define AM_HAL_LAU_STATUS_ERROR_SUSTAINED_UNDERFLOW         (1 << 2)
#define AM_HAL_LAU_STATUS_ERROR_INVALID_PAIR_PARITY         (1 << 3)
#define AM_HAL_LAU_STATUS_ERROR_ALL                         0x0F

//
//! Software clear mask for STATUS.BNDMISSED (v2.62, write-1-to-clear).
//! Not part of ui32Error / ERRFLAGS; pass to am_hal_lau_status_error_clear().
//!
#define AM_HAL_LAU_STATUS_BND_MISSED                        (1u << 4)
#define AM_HAL_LAU_STATUS_CLEAR_ALL                         \
    (AM_HAL_LAU_STATUS_ERROR_ALL | AM_HAL_LAU_STATUS_BND_MISSED)

//
//! LAU interrupt status / enable bit masks (CMSIS LAU_IRQSTAT_* / LAU_IRQEN_*).
//! Only ERR[0] (AXI MEM error) raises the ERR interrupt; ERR[1..3] set
//! STATUS.ERRFLAGS without interrupting (guide section 8).
//
#define AM_HAL_LAU_INT_VSYNC        LAU_IRQSTAT_VSYNC_Msk
#define AM_HAL_LAU_INT_LINEMATCH    LAU_IRQSTAT_LINEMATCH_Msk
#define AM_HAL_LAU_INT_ERR          LAU_IRQSTAT_ERR_Msk
#define AM_HAL_LAU_INT_ALL          (AM_HAL_LAU_INT_VSYNC | AM_HAL_LAU_INT_LINEMATCH | AM_HAL_LAU_INT_ERR)
//
//! PIXFMT register values.
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
    AM_HAL_LAU_DISP_MODE_MAX,
} am_hal_lau_disp_port_mode_e;

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
    bool bDoubleBufferEnable;       //!< Enable CTRL.DBLBUFEN.
    bool bAutoSwapEnable;           //!< Enable CTRL.AUTOSWAP.
} am_hal_lau_buffering_cfg_t;

//
//! LAU DC Clock Source
//
typedef enum
{
    AM_HAL_LAU_DC_CLK_MUX_OFF       = 0,  //!< 00: output tied low.
    AM_HAL_LAU_DC_CLK_MUX_DISP_PLL  = 1,  //!< 01: disp_pll_clk.
    AM_HAL_LAU_DC_CLK_MUX_DSI_HS    = 2,  //!< 10: dsi_hs_clk.
    AM_HAL_LAU_DC_CLK_MUX_TIED_HIGH = 3,  //!< 11: output tied high.
} am_hal_lau_dc_clk_mux_e;

//
//! LAU Framebuffer Windows
//
typedef enum
{
    AM_HAL_LAU_FRAMEBUFFER_0 = 0,
    AM_HAL_LAU_FRAMEBUFFER_1 = 1,
    AM_HAL_LAU_FRAMEBUFFER_NUM,
} am_hal_lau_framebuffer_e;

//
//! LAU Framebuffer WindowConfiguration
//! @note DISP port (Display Controller):
//!   Both FB_WIN0_LIN_BASE and FB_WIN1_LIN_BASE access the same internal
//!   scanline buffer. The prefetch data source is controlled solely by
//!   buf_sel (double-buffer swap), not by which LIN_BASE the DC reads.
//!   DC layers should always use AM_HAL_LAU_DC_LAYER_ADDR.
//!
//! @note CPU port:
//!   FB_WIN0_LIN_BASE and FB_WIN1_LIN_BASE independently map to
//!   FB_WIN0_FB_BASE and FB_WIN1_FB_BASE tile memory respectively.
//!   CPU access is unaffected by buf_sel or dbl_buf_en.
//!   Use am_hal_lau_fb_cpu_read/write with eFrameBuffer to select.
//
typedef struct
{
    //!< Tiled framebuffer base address.
    //! Maps to FBWINxFBBASE register.
    uint32_t ui32TileBase;

    //!< Linear aperture base address.
    //! Maps to FBWINxLINBASE register.
    //! Must be 8-byte aligned.
    uint32_t ui32LinBase;

    //!< Linear aperture size in bytes.
    //! Maps to FBWINxSIZE register.
    //! ui32Size of 0 selects bridge mode (FBWINxSIZE = 0; no CPU linear aperture).
    uint32_t ui32Size;
} am_hal_lau_fbwin_cfg_t;

//
//! LAU surface configuration.
//!
//! Describes the source framebuffer surface.
//! The HAL uses this information to program LAU surface pitch and tile-row
//! stride registers.
//!
//! Note: LAU hardware also has a STRIDE register, but that register represents
//! tile-row stride, not line pitch. The HAL performs the conversion
//! internally.
//
typedef struct
{
    am_hal_lau_pix_fmt_e ePixFmt;   //!< Pixel format of the framebuffer surface.
    uint32_t ui32WidthPixels;       //!< Valid surface width in pixels, excluding padding.
    uint32_t ui32HeightPixels;      //!< Valid surface height in pixels.

    //!< Line pitch in bytes (maps to SURFACE_PITCH).
    //! Use AM_HAL_LAU_SURFACE_PITCH_ALIGN(width, bpp), or a larger padded
    //! pitch. Must be >= width * bytes_per_pixel and 32-byte aligned.
    uint32_t ui32PitchBytes;
} am_hal_lau_surface_cfg_t;

//
//! LAU Configuration
//!
//! Framebuffer-window rules:
//! - Bridge-only single-buffer operation may leave both framebuffer windows
//!   disabled (ui32Size = 0).
//! - LAU framebuffer operation requires FBWin0 to be enabled and valid.
//! - Double-buffer operation requires both FBWin0 and FBWin1 to be enabled,
//!   valid, and non-overlapping.
//! - AUTO_SWAP requires a valid double-buffer configuration.
//
typedef struct
{
    //! GPU framebuffer surface configuration.
    am_hal_lau_surface_cfg_t sSurface;

    //! Framebuffer window configuration (tile base, linear aperture base/size).
    //! sFBWin[AM_HAL_LAU_FRAMEBUFFER_0] is always used when configured.
    //! sFBWin[AM_HAL_LAU_FRAMEBUFFER_1] is used when double buffering is enabled.
    am_hal_lau_fbwin_cfg_t sFBWin[AM_HAL_LAU_FRAMEBUFFER_NUM];

    //! Source pixel window within the GPU surface (WINX/Y/W/H).
    //! Does not program FBWINxSIZE; use sFBWin[].ui32Size for the aperture.
    am_hal_lau_window_cfg_t sWindow;

    //! Single/double buffer and swap policy.
    am_hal_lau_buffering_cfg_t sBuffering;

    //!< Display port mode.
    am_hal_lau_disp_port_mode_e eDispMode;

    //!< MiP (memory-in-pixel) command-mode contract (CTRL.MIPMODE).
    //! Meaningful when eDispMode is command mode; default false.
    bool bMipMode;

    //!< MiP frame-envelope backup boundary enable (CTRL.MIPENVEN).
    bool bMipEnvEnable;
} am_hal_lau_config_t;

//
//! LAU Status
//
typedef struct
{
    bool bBusy;                                  //!< LAU busy status.
    bool bDispActive;                            //!< LAU Display linear access in progress
    am_hal_lau_framebuffer_e eActiveFramebuffer; //!< LAU active framebuffer.
    bool bSwapPending;                           //!< SW buffer swap armed and waiting for commit.
    uint32_t ui32CurScanline;                    //!< LAU current scanline.
    uint32_t ui32Error;                          //!< Unshifted STATUS.ERRFLAGS.
    bool bBndMissed;                             //!< STATUS.BNDMISSED (command-mode update boundary missed).
    uint32_t ui32ReAnchorCnt;                    //!< STATUS.REANCHORCNT (prefetch re-anchors in open update).
} am_hal_lau_status_t;

//
//! LAU Request
//
typedef enum
{
    //! Pass bool pointer as pArgs to enable or disable auto swap.
    //! Enabling auto swap requires double buffering to be enabled.
    AM_HAL_LAU_REQ_SET_AUTO_SWAP,
    //! Pass bool pointer as pArgs to enable or disable double buffering.
    //! Enabling double buffering requires FBWin1 to be configured.
    AM_HAL_LAU_REQ_SET_DOUBLE_BUFFER,
    //! Arm a software buffer swap (CMD.SWBUFSWAP).
    //! Requires FBWin1 to be configured and no swap already pending;
    //! otherwise returns AM_HAL_STATUS_INVALID_OPERATION.
    //! pArgs N/A
    AM_HAL_LAU_REQ_SET_SW_BUF_SWAP,
    //! Complete pending CPU aperture writes using the configured CAR-4265
    //! write-combine completion sequence.
    //! - Option A (default): DMB, CMD.WCFLUSH, DMB, one 32-bit aperture read,
    //!   DMB. Call with interrupts masked and no concurrent aperture access.
    //! - Option B: DMB, 32-bit reads at lines 0 and 8, DMB. Requires WIN_H >= 9.
    //! Pass am_hal_lau_framebuffer_e pointer as pArgs (same window as
    //! am_hal_lau_fb_cpu_write()). The window must be enabled (SIZE != 0).
    AM_HAL_LAU_REQ_SET_WC_FLUSH,
    //! Command-mode update boundary (CMD.UPDATEEND).
    //! Write after DC finishes an update; clears / avoids STATUS.BNDMISSED.
    //! pArgs N/A
    AM_HAL_LAU_REQ_SET_UPDATE_END,
    //! Display port mode set (CTRL.MODE).
    //! Requires LAU to be disabled.
    //! Pass am_hal_lau_disp_port_mode_e pointer as pArgs.
    AM_HAL_LAU_REQ_SET_DISP_MODE,
    //! MiP mode set (CTRL.MIPMODE).
    //! Requires LAU to be disabled.
    //! Pass bool pointer as pArgs.
    AM_HAL_LAU_REQ_SET_MIP_MODE,
    //! MiP envelope enable set (CTRL.MIPENVEN).
    //! Requires LAU to be disabled.
    //! Pass bool pointer as pArgs.
    AM_HAL_LAU_REQ_SET_MIP_ENV_ENABLE,
    //! Set line IRQ Y (LINEIRQY.LINEY). Must be no greater than 0xFFFF and a
    //! multiple of 8 or equal to WIN_Y (other values never match the prefetch
    //! position).
    //! Pass uint32_t as pArgs.
    AM_HAL_LAU_REQ_SET_LINE_IRQ_Y,
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
//! The hardware validates the configuration when CTRL.EN is set. If the
//! configuration is rejected (STATUS.ERR[3], invalid config / pair-parity
//! reject), the enable is rolled back (CTRL.EN = 0) and AM_HAL_STATUS_FAIL
//! is returned.
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
//! This function disables the LAU and immediately issues CMD.SOFTRST
//! (CAR-4265). The caller must stop the display controller at a frame boundary
//! and prevent framebuffer accesses until this function returns.
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
//! sFBWin[].ui32LinBase must be non-zero when ui32Size is non-zero.
//! sFBWin[].ui32Size of 0 selects bridge mode for FB0 and FB1 (FBWINxSIZE = 0;
//! display uses the MEM/bridge path, no CPU linear aperture).
//! See am_hal_lau_config_t for the single-buffer, double-buffer, and AUTO_SWAP
//! framebuffer-window rules.
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
//! This function is currently unsupported and returns
//! AM_HAL_STATUS_INVALID_OPERATION. It does not power off the LAU.
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
//! @brief Clear LAU STATUS sticky flags.
//!
//! @param ui32ErrorMask - Combination of:
//!   - AM_HAL_LAU_STATUS_ERROR_* (unshifted ERRFLAGS, as in ui32Error)
//!   - AM_HAL_LAU_STATUS_BND_MISSED (clears STATUS.BNDMISSED)
//!
//! STATUS.ERRFLAGS and STATUS.BNDMISSED are write-1-to-clear.
//! Note: ERR[2] (sustained underflow / port stall) typically requires a
//! soft reset to recover (guide sections 6.1 and 11).
//*****************************************************************************
extern uint32_t am_hal_lau_status_error_clear(void *pHandle, uint32_t ui32ErrorMask);

//*****************************************************************************
//
//! @brief LAU Wait until the LAU engine is no longer busy.
//!
//! Polls STATUS.BUSY (bit 0) until it is cleared.
//! Use only when the hardware operation explicitly permits polling. Do not
//! call it between clearing CTRL.EN and issuing CMD.SOFTRST: CAR-4265 requires
//! those writes to be consecutive. WC_FLUSH provides completion through its
//! aperture readback sequence.
//!
//! @param pHandle      - handle for the interface.
//! @param ui32TimeoutUs - timeout in microseconds.
//!
//!
//! @return status      - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_wait_not_busy(void *pHandle, uint32_t ui32TimeoutUs);

//*****************************************************************************
//
//! @brief LAU framebuffer window update
//!
//! @param pHandle        - handle for the interface.
//! @param eFrameBuffer   - framebuffer index.
//! @param pFbWin         - pointer to FBWINx update (FBBASE/LINBASE/SIZE).
//!
//! Update of one FBWINx without configure() or soft reset. Call after
//! am_hal_lau_configure(). Does not modify WIN_* or surface registers.
//!
//! All fields in pFbWin are taken directly from the caller. When ui32Size is
//! non-zero, ui32TileBase and ui32LinBase must be non-zero, ui32TileBase must
//! be 64 x bytes-per-pixel aligned, ui32LinBase must be 8-byte aligned, and
//! ui32Size must equal WIN_W x WIN_H x bytes_per_pixel.
//!
//! ui32Size of 0: FB0 selects bridge mode (FBWIN0SIZE = 0).
//!
//! AM_HAL_LAU_FRAMEBUFFER_1: ui32Size of 0 at runtime clears FBWIN1.
//! Use AM_HAL_LAU_REQ_SET_DOUBLE_BUFFER to enable or disable double buffering.
//! Clearing FBWIN1 is rejected with AM_HAL_STATUS_INVALID_OPERATION while
//! double buffering is enabled; disable double buffering first.
//!

//!
//! @return status        - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_fbwin_set(void *pHandle,
                                     am_hal_lau_framebuffer_e eFrameBuffer,
                                     const am_hal_lau_fbwin_cfg_t *pFbWin);

//*****************************************************************************
//
//! @brief LAU framebuffer window query function
//!
//! @param pHandle        - handle for the interface.
//! @param eFrameBuffer   - framebuffer index.
//! @param pFbWin         - pointer to FBWINx configuration populated from cache.
//!
//! Returns the cached FBWINx configuration from the most recent configure() or
//! fbwin_set() call.
//!
//! @return status        - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_fbwin_get(void *pHandle,
                                     am_hal_lau_framebuffer_e eFrameBuffer,
                                     am_hal_lau_fbwin_cfg_t *pFbWin);

//*****************************************************************************
//
//! @brief LAU enable interrupts function
//!
//! @param pHandle      - handle for the interface.
//! @param ui32IntMask  - interface specific interrupt mask.
//!
//! This function enables the specific indicated interrupts.
//!
//! @return status      - generic or interface specific status.
//
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
extern uint32_t am_hal_lau_interrupt_status_get(void *pHandle, uint32_t *pui32IntStatus,
                                                bool bEnabledOnly);

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
//! @param pHandle        - handle for the interface.
//! @param ui32IntStatus  - interrupt status flags from LAU->IRQSTAT.
//!
//! This function is designed to be called from within the user-defined ISR
//! after reading LAU->IRQSTAT. Pass the masked interrupt status to be serviced.
//!
//! @return status        - generic or interface specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_interrupt_service(void *pHandle, uint32_t ui32IntStatus);

//*****************************************************************************
//
//! @brief Read data from a framebuffer through the LAU CPU linear access path.
//!
//! @param pHandle      - Handle for the LAU instance.
//! @param eFrameBuffer - Framebuffer index. Selects the corresponding LAU
//!                       linear access window (FB_WINx_LIN_BASE).
//! @param ui32Offset   - Byte offset from the selected linear access base address.
//! @param pui8Data     - Pointer to the destination linear buffer.
//! @param ui32NumBytes - Number of bytes to read.
//!
//! The selected framebuffer is accessed through the LAU CPU linear access
//! window. LAU converts the tiled framebuffer layout into linear raster-order
//! data and stores the result into the destination buffer.
//!
//! @note The CPU aperture is available regardless of CTRL.EN; the LAU must
//! still be initialized and the selected window configured.
//!
//! @return status      - Generic or interface-specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_fb_cpu_read(void *pHandle,
                                       am_hal_lau_framebuffer_e eFrameBuffer,
                                       uint32_t ui32Offset,
                                       uint8_t *pui8Data,
                                       uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief Write data to a framebuffer through the LAU CPU linear access path.
//!
//! @param pHandle      - Handle for the LAU instance.
//! @param eFrameBuffer - Framebuffer index. Selects the corresponding LAU
//!                       linear access window (FB_WINx_LIN_BASE).
//! @param ui32Offset   - Byte offset from the selected linear access base address.
//! @param pui8Data     - Pointer to the source linear buffer.
//! @param ui32NumBytes - Number of bytes to write.
//!
//! The selected framebuffer is accessed through the LAU CPU linear access
//! window. LAU converts the linear raster-order data into the tiled framebuffer
//! layout and writes the result into the corresponding framebuffer memory.
//!
//! @note The CPU aperture is available regardless of CTRL.EN; the LAU must
//! still be initialized and the selected window configured. Issue
//! AM_HAL_LAU_REQ_SET_WC_FLUSH with the same eFrameBuffer after writing so
//! that the DC observes the new data.
//!
//! @return status      - Generic or interface-specific status.
//
//*****************************************************************************
extern uint32_t am_hal_lau_fb_cpu_write(void *pHandle,
                                        am_hal_lau_framebuffer_e eFrameBuffer,
                                        uint32_t ui32Offset,
                                        const uint8_t *pui8Data,
                                        uint32_t ui32NumBytes);

//*****************************************************************************
//
//! @brief LAU set the DC clock mux
//!
//! @param pHandle      - handle for the interface.
//! @param eClkMux      - DC clock mux selection.
//!
//! This function sets the DC clock mux.
//!
//! Must be called before am_hal_lau_configure(); the selection is carried in
//! every subsequent CTRL write (LAU-SW-03). Change the selection only while
//! the DC is stopped (LAU-SW-06). This function rejects the request while
//! CTRL.EN is set; software must also stop bridge-mode DC traffic while
//! CTRL.EN is clear.
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
