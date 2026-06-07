//*****************************************************************************
//
//! @file am_hal_sysctrl_clk_mux_reset.h
//!
//! @brief Internal api definition for Clock Mux Reset Operation
//!
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
//! @cond SYSCTRL_CLK_MUX_RESET
#ifndef AM_HAL_SYSCTRL_CLK_MUX_RESET_H
#define AM_HAL_SYSCTRL_CLK_MUX_RESET_H

#ifdef __cplusplus
extern "C"
{
#endif

#define HALSTATE        STIMER->HALSTATES
#define HALSTATE_b      STIMER->HALSTATES_b
#define SYSCTRL_CLKMUXRST_SIGNATURE 0x5AF0

//*****************************************************************************
//
//! CLKMUXRST Clock Source Bitmap
//
//*****************************************************************************
#define SYSCTRL_CLKMUXRST_CLK_CLKGEN        0x00
#define SYSCTRL_CLKMUXRST_CLK_HFRC_DED      0x01
#define SYSCTRL_CLKMUXRST_CLK_HFRC2         0x02
#define SYSCTRL_CLKMUXRST_CLK_XTAL          0x04
#define SYSCTRL_CLKMUXRST_CLK_EXTCLK        0x08
#define SYSCTRL_CLKMUXRST_CLK_PLL           0x10

//*****************************************************************************
//
//! CLKMUXRST clock mux enum
//
//*****************************************************************************
typedef enum
{
    SYSCTRL_CLKMUXRST_MUX_CG_AUDADC,
    SYSCTRL_CLKMUXRST_MUX_LL_AUDADC,
    SYSCTRL_CLKMUXRST_MUX_LL_PDM,
    SYSCTRL_CLKMUXRST_MUX_LL_I2S0,
    SYSCTRL_CLKMUXRST_MUX_LL_I2S1,
    SYSCTRL_CLKMUXRST_MUX_LL_USB,

    // SYSCTRL_CLKMUXRST_CLK_MAX Should always be the last entry
    SYSCTRL_CLKMUXRST_CLK_MAX,
}
am_hal_sysctrl_clkmuxrst_clk_e;

//*****************************************************************************
//
//! @brief Update clocks needed for clock muxes reset operation
//!
//! @param bClkgenOff - AUDADC ClockGen clock selection is OFF
//
//*****************************************************************************
extern void am_hal_sysctrl_clkmuxrst_audadc_clkgen_off_update(bool bClkgenOff);

//*****************************************************************************
//
//! @brief Update clocks needed for clock muxes reset operation
//!
//! @param eFref - Reference Clock select for PLL
//
//*****************************************************************************
extern void am_hal_sysctrl_clkmuxrst_pll_fref_update(MCUCTRL_PLLCTL0_FREFSEL_Enum eFref);

//*****************************************************************************
//
//! @brief Update PLL FREF selection
//!
//! @param eClk - am_hal_sysctrl_clkmuxrst_clk_e value for mux to update.
//! @param ui8ClkSrcBm - bitmap representing clocks needed by the mux
//
//*****************************************************************************
extern void am_hal_sysctrl_clkmuxrst_clkneeded_update(am_hal_sysctrl_clkmuxrst_clk_e eClk, uint8_t ui8ClkSrcBm);

//*****************************************************************************
//
//! @brief Handle clock muxes reset during low_power_init
//
//*****************************************************************************
extern void am_hal_sysctrl_clkmuxrst_low_power_init(void);

#ifdef __cplusplus
}
#endif

#endif //AM_HAL_SYSCTRL_CLK_MUX_RESET_H

//! @endcond SYSCTRL_CLK_MUX_RESET
