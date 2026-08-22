//*****************************************************************************
//
//! @file am_hal_crm_private.h
//!
//! @brief Clock and Reset Management (CRM) functionality implementation.
//!
//! @addtogroup crm_at110 Clock and Reset Management (CRM)
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
//! @cond CRM_HAL_PRIVATE_FUNC
#ifndef AM_HAL_CRM_PRIVATE_H
#define AM_HAL_CRM_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

// #### INTERNAL BEGIN ####
// Confirmed with DV, that below 5 mux are unused for now,
//(PLLMFREF, PLLSFREF, SECUREWDT, WDT SRAM)
// FREF selection should be done from MCU_CTRL.xxxPLLCTL instead for now.
//#define AM_HAL_CRM_ENABLE_PLLCLKCFG
//#define AM_HAL_CRM_ENABLE_WDTCLKCFG
//#define AM_HAL_CRM_ENABLE_SSRAMCLKCFG
// #### INTERNAL END ####
//=============================================================================
// CRM block - Clock domain ID enumeration definitions
//=============================================================================
typedef enum
{
    AM_HAL_CRM_ID_ADC,
    AM_HAL_CRM_ID_DISPPLL,
    AM_HAL_CRM_ID_DSIESC,
    AM_HAL_CRM_ID_DSISYS,
    AM_HAL_CRM_ID_DSIULPS,
    AM_HAL_CRM_ID_DSIPHYREF,
    AM_HAL_CRM_ID_DSIPHYTX,
    AM_HAL_CRM_ID_EMMCXIN,
    AM_HAL_CRM_ID_HBLMC,
    AM_HAL_CRM_ID_I2S0M,
    AM_HAL_CRM_ID_I2S1M,
    AM_HAL_CRM_ID_I3C0REF,
    AM_HAL_CRM_ID_I3C1REF,
    AM_HAL_CRM_ID_I3C2REF,
    AM_HAL_CRM_ID_IOM0,
    AM_HAL_CRM_ID_IOM1,
    AM_HAL_CRM_ID_IOM2,
    AM_HAL_CRM_ID_IOM3,
    AM_HAL_CRM_ID_IOM4,
    AM_HAL_CRM_ID_IOM5,
    AM_HAL_CRM_ID_IOM6,
    AM_HAL_CRM_ID_IOM7,
    AM_HAL_CRM_ID_IOM8,
    AM_HAL_CRM_ID_IOM9,
    AM_HAL_CRM_ID_IOM10,
    AM_HAL_CRM_ID_IOM11,
    AM_HAL_CRM_ID_PDM0,
    AM_HAL_CRM_ID_PDM1,
    AM_HAL_CRM_ID_PDM2,
    AM_HAL_CRM_ID_SDIO0XIN,
    AM_HAL_CRM_ID_UART0HF,
    AM_HAL_CRM_ID_UART1HF,
    AM_HAL_CRM_ID_UART2HF,
    AM_HAL_CRM_ID_UART3HF,
    AM_HAL_CRM_ID_UART4HF,
    AM_HAL_CRM_ID_UART5HF,
    AM_HAL_CRM_ID_USBPHYCORE,
    AM_HAL_CRM_ID_XSPI0PHY,
    AM_HAL_CRM_ID_XSPI1PHY,
    AM_HAL_CRM_ID_XSPI2PHY,

    // Keep this as the last element in the enum
    AM_HAL_CRM_ID_COUNT,
} am_hal_crm_id_e;

//=============================================================================
// CLKCFG mux block - Clock domain ID enumeration definitions
//=============================================================================
// #### INTERNAL BEGIN ####
// For now we are only exposing those instance with clock select for clock
// source switching.
// #### INTERNAL END ####
typedef enum
{
    // #### INTERNAL BEGIN ####
    // CLKSEL is marked unused for this register in reggen spec
    // AM_HAL_CRM_CLKCFG_MUX_ID_APBDMAAPB,
    // TODO: remove DISPAXI and GFX as AXI is no longer selectable
    // TODO: remove GFXCORE as it is going to be controlled by MCU_CTRL
    // #### INTERNAL END ####
    AM_HAL_CRM_CLKCFG_MUX_ID_DISPAXI,
    AM_HAL_CRM_CLKCFG_MUX_ID_GFXAXI,
    AM_HAL_CRM_CLKCFG_MUX_ID_GFXCORE,
    AM_HAL_CRM_CLKCFG_MUX_ID_HBLSIFM,
    AM_HAL_CRM_CLKCFG_MUX_ID_I2S0ASRC,
    AM_HAL_CRM_CLKCFG_MUX_ID_I2S0NCO,
    AM_HAL_CRM_CLKCFG_MUX_ID_I2S1NCO,
    // #### INTERNAL BEGIN ####
#ifdef AM_HAL_CRM_ENABLE_PLLCLKCFG
    AM_HAL_CRM_CLKCFG_MUX_ID_PLLMFREF,
    AM_HAL_CRM_CLKCFG_MUX_ID_PLLSFREF,
#endif
#ifdef AM_HAL_CRM_ENABLE_WDTCLKCFG
    AM_HAL_CRM_CLKCFG_MUX_ID_SECUREWDT,
    AM_HAL_CRM_CLKCFG_MUX_ID_WDT,
#endif
#ifdef AM_HAL_CRM_ENABLE_SSRAMCLKCFG
    AM_HAL_CRM_CLKCFG_MUX_ID_SSRAM,
#endif
    // #### INTERNAL END ####
    AM_HAL_CRM_CLKCFG_MUX_ID_TPIU,
    // Keep this as the last element in the enum
    AM_HAL_CRM_CLKCFG_MUX_ID_COUNT,
} am_hal_crm_clkcfg_mux_id_e;

//=============================================================================
// API Definitions
//=============================================================================
//*****************************************************************************
//
//! @brief CRM initialization.
//!
//! @return Status of the operation.
//
//*****************************************************************************
extern uint32_t am_hal_crm_initialize(void);

//*****************************************************************************
//
//! @brief CRM clock enable control.
//!
//! @param eCrmId: CRM ID.
//! @param bEnable: True to enable, false to disable.
//! @param ui32ClkSel: Clock selection.
//! @param ui32Clkdiv: Clock division.
//!
//! @return Status of the operation.
//!         - AM_HAL_STATUS_SUCCESS: Operation completed successfully.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid CRM ID or clock
//!           selector/divider value out of valid range.
//!         - AM_HAL_STATUS_TIMEOUT: Timeout waiting for clock to become active
//!           or inactive.
//
//*****************************************************************************
extern uint32_t am_hal_crm_clock_enable(am_hal_crm_id_e eCrmId, bool bEnable, uint32_t ui32ClkSel, uint32_t ui32Clkdiv);

//*****************************************************************************
//
//! @brief CRM clock reset control.
//!
//! @param eCrmId: CRM ID.
//! @param bReset: True to assert reset, false to release reset.
//!
//! @return Status of the operation.
//
//*****************************************************************************
extern uint32_t am_hal_crm_reset_enable(am_hal_crm_id_e eCrmId, bool bReset);

//*****************************************************************************
//
//! @brief CLKCFG mux clock select control.
//!
//! @param eMuxId: CLKCFG mux ID.
//! @param ui32ClkSel: Clock selection.
//!
//! @return Status of the operation.
//!         - AM_HAL_STATUS_SUCCESS: Operation completed successfully.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid CLKCFG mux ID or clock
//!           selector value out of valid range.
//
//*****************************************************************************
extern uint32_t am_hal_crm_clkcfg_mux_config(am_hal_crm_clkcfg_mux_id_e eMuxId, uint32_t ui32ClkSel);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_CRM_PRIVATE_H
//! @endcond CRM_HAL_PRIVATE_FUNC

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
