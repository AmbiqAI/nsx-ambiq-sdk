//*****************************************************************************
//
//! @file am_hal_syspll.h
//!
//! @brief Functions for interfacing with the System PLL.
//!
//! @addtogroup syspll_at110 SYSPLL - System PLL
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
#ifndef AM_HAL_SYSPLL_H
#define AM_HAL_SYSPLL_H

#ifdef __cplusplus
extern "C"
{
#endif


//*****************************************************************************
//
//! @brief SYSPLL Module Number definition
//
//*****************************************************************************
#define AM_HAL_SYSPLL_MOD_NUM_SYSCLK 0
#define AM_HAL_SYSPLL_MOD_NUM_MEMPLL 1

//*****************************************************************************
//
// System PLL Limit values
//
//*****************************************************************************
#define AM_HAL_SYSPLL_MAX_REFDIV                (63U)
#define AM_HAL_SYSPLL_MIN_FBDIV_INT_MODE        (16U)
#define AM_HAL_SYSPLL_MAX_FBDIV_INT_MODE        (4095U)
#define AM_HAL_SYSPLL_MIN_FBDIV_FRAC_MODE       (20U)
#define AM_HAL_SYSPLL_MAX_FBDIV_FRAC_MODE       (4095U)
#define AM_HAL_SYSPLL_MAX_FBDIV_FRACTIONAL      (0xFFFFFFU)
#define AM_HAL_SYSPLL_VCO_FREQ_MIN_MHZ          (500U)
#define AM_HAL_SYSPLL_VCO_FREQ_MAX_MHZ          (1000U)
#define AM_HAL_SYSPLL_REF_FREQ_MIN_MHZ          (4U)
#define AM_HAL_SYSPLL_REF_FREQ_MAX_MHZ          (250U)
#define AM_HAL_SYSPLL_FPFD_FREQ_MIN_MHZ         (4U)
#define AM_HAL_SYSPLL_FPFD_FREQ_MAX_MHZ         (50U)
#define AM_HAL_SYSPLL_POST_DIV_MIN              (1U)
#define AM_HAL_SYSPLL_POST_DIV_MAX              (32U)
#define AM_HAL_SYSPLL_SA_DIV_MIN                (2U)
#define AM_HAL_SYSPLL_SA_DIV_MAX                (128U)

#ifndef AM_HAL_SYSPLL_DEFAULT_PPM
#define AM_HAL_SYSPLL_DEFAULT_PPM               (100U)
#endif

// ****************************************************************************
//
//! @enum am_hal_syspll_vddfpdn_e
//! @brief System PLL VDDF power down,when enable vddf also enable vddh
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_VDDF_POWER_ON     = MCUCTRL_SYSPLLCTL_SYSPLLVDDFPDN_ENABLE,
    AM_HAL_SYSPLL_VDDF_POWER_OFF    = MCUCTRL_SYSPLLCTL_SYSPLLVDDFPDN_DISABLE,
} am_hal_syspll_vddfpdn_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_en_e
//! @brief System PLL Enable
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_EN_DISABLE    = MCUCTRL_SYSPLLCTL_SYSPLLEN_DISABLE,
    AM_HAL_SYSPLL_EN_ENABLE     = MCUCTRL_SYSPLLCTL_SYSPLLEN_ENABLE,
} am_hal_syspll_en_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_bypass_e
//! @brief System PLL bypass select.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_BYPASS_NORMAL = MCUCTRL_SYSPLLCTL_SYSPLLBYPASS_NORMAL,
    AM_HAL_SYSPLL_BYPASS_BYPASS = MCUCTRL_SYSPLLCTL_SYSPLLBYPASS_BYPASS,
} am_hal_syspll_bypass_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_frefsel_e
//! @brief System PLL reference clock select.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_FREFSEL_HFRCDIV10    = MCUCTRL_SYSPLLCTL_SYSPLLFREFSEL_HFRCDIV10,
    AM_HAL_SYSPLL_FREFSEL_EXTREFCLK    = MCUCTRL_SYSPLLCTL_SYSPLLFREFSEL_EXTREFCLK,
    AM_HAL_SYSPLL_FREFSEL_XTAL48MHz    = MCUCTRL_SYSPLLCTL_SYSPLLFREFSEL_HFXTAL48MHZ,
} am_hal_syspll_frefsel_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_dacen_e
//! @brief System PLL noise canceling DAC ENABLE
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_DACEN_ACTIVE      = MCUCTRL_SYSPLLCTL_SYSPLLDACEN_ACTIVE,
    AM_HAL_SYSPLL_DACEN_NOT_ACTIVE  = MCUCTRL_SYSPLLCTL_SYSPLLDACEN_NOT_ACTTIVE,
} am_hal_syspll_dacen_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_fraction_mode_e
//! @brief System PLL delta-sigma modulator powerdown.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_FMODE_FRACTION = MCUCTRL_SYSPLLCTL_SYSPLLDSMEN_ACTIVE,
    AM_HAL_SYSPLL_FMODE_INTEGER  = MCUCTRL_SYSPLLCTL_SYSPLLDSMEN_POWERDOWN,
    AM_HAL_SYSPLL_FMODE_MAX      = 1U,
} am_hal_syspll_fraction_mode_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_foutpostdiven_output_enable_e
//! @brief System PLL post divider output enable, connected to PLL pin FOUTPOSTDIVEN.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_FOUTPOSTDIVEN_ACTIVE      = MCUCTRL_SYSPLLCTL_SYSPLLFOUTPOSTDIVEN_ACTIVE,
    AM_HAL_SYSPLL_FOUTPOSTDIVEN_POWERDOWN   = MCUCTRL_SYSPLLCTL_SYSPLLFOUTPOSTDIVEN_POWERDOWN,
} am_hal_syspll_foutpostdiven_output_enable_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_foutvcoen_output_enable_e
//! @brief System PLL VCO rate output clock enable
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_FOUTVCOEN_ACTIVE      = MCUCTRL_SYSPLLCTL_SYSPLLFOUTVCOEN_ACTIVE,
    AM_HAL_SYSPLL_FOUTVCOEN_POWERDOWN   = MCUCTRL_SYSPLLCTL_SYSPLLFOUTVCOEN_POWERDOWN,
} am_hal_syspll_foutvcoen_output_enable_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_lock_status_e
//! @brief System PLL frequency lock signal.
//! Indicates no cycle slip between the feedback clock and FPFD for 128 consecutive cycles;
//
// ****************************************************************************
// TODO: update this when the register definition is available
typedef enum
{
    AM_HAL_SYSPLL_LOCK_STATUS_LOCKED        = 0,
    AM_HAL_SYSPLL_LOCK_STATUS_NOT_LOCKED    = 1,
} am_hal_syspll_lock_status_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_emphasis_mode_e
//! @brief System PLL emphasis mode selection when generating PLL
//         configuration.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_FAVOR_POWER,
    AM_HAL_SYSPLL_FAVOR_LOCK_SPEED,
} am_hal_syspll_emphasis_mode_e;

// ****************************************************************************
//
//! @enum am_hal_syspll_sysclk_output_id_e
//! @brief System PLL output ID for SYS_CLK PLL.
//
// ****************************************************************************
typedef enum
{
    AM_HAL_SYSPLL_OUTPUT_SYS_CLK0,
    AM_HAL_SYSPLL_OUTPUT_SYS_CLK1,
    AM_HAL_SYSPLL_OUTPUT_SYS_CLK2,
    AM_HAL_SYSPLL_OUTPUT_SYS_CLK3,
    AM_HAL_SYSPLL_OUTPUT_SYS_CLK4,
} am_hal_syspll_sysclk_output_id_e;

//*****************************************************************************
//
//! @struct am_hal_syspll_config_t
//! @brief Configuration structure for the System PLL @n
//!     PLL Output Clock rates calculation:
//!     - FBDiv = ui16FBDivInt + (ui32FBDivFrac / (2^24))
//!     - PLL VCO Frequncy, Fvco = (Fref * FBDiv / ui8RefDiv)
//
//*****************************************************************************
typedef struct
{
    //! Select System PLL reference clock.
    am_hal_syspll_frefsel_e         eFref;

    //! System PLL Fraction Mode Select
    am_hal_syspll_fraction_mode_e   eFractionMode;

    //! System PLL Reference clock divide value.
    uint8_t                         ui8RefDiv;

    //! System PLL post divide value.
    uint8_t                         ui8PostDiv;

    //! System PLL feedback divide value integer part.
    uint16_t                        ui16FBDivInt;

    //! System PLL feedback divide value fractional part.
    uint32_t                        ui32FBDivFrac;
} am_hal_syspll_config_t;

//*****************************************************************************
//
//! @brief Generate system PLL configuration to achieve desired VCO Frequency @n
//! This function generates the following configurations for system PLL.
//! configuration not listed needs to be set in separately.
//! - eFractionMode
//! - ui8RefDiv
//! - ui16FBDivInt
//! - ui32FBDivFrac
//!
//! @param pConfig        - pointer to the system PLL Config structure.
//! @param ui32RefClk_Hz  - system PLL reference clock frequency in Hz
//! @param ui32VCOClk_Hz  - system PLL VCO output frequency desired in Hz
//! @param eEmphasis      - emphasis mode for PLL configuration selection
//!
//! @return status        - status whether valid configuration is generated:
//! - AM_HAL_STATUS_SUCCESS: configuration succesfully generated
//! - AM_HAL_STATUS_INVALID_ARG: invalid arguments received or generated
//!                               configuration exceeds tolerance limit.
//! - AM_HAL_STATUS_OUT_OF_RANGE: ui32VCOClk_Hz or ui32RefClk_Hz out of range.
//! - AM_HAL_STATUS_FAIL: No valid configuration found.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_config_generate(am_hal_syspll_config_t* pConfig, uint32_t ui32RefClk_Hz, uint32_t ui32VCOClk_Hz, am_hal_syspll_emphasis_mode_e eEmphasis);

//*****************************************************************************
//
//! @brief System PLL initialization function
//!
//! @param ui32Module   - module instance.
//! @param handle       - pointer to handle pointer for the module instance.
//!
//! This function accepts a module instance, allocates the interface and then
//! returns a handle to be used by the remaining interface functions.
//!
//! @return status      - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL initialized succesfully
//!         - AM_HAL_STATUS_OUT_OF_RANGE: Invalid ui32Module.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid ppHandle.
//!         - AM_HAL_STATUS_INVALID_OPERATION: Module already initialized.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief System PLL deinitialization function
//!
//! @param pHandle      - pointer to handle pointer for the module instance.
//!
//! This function accepts a handle to an instance and de-initializes the
//! interface.
//!
//! @return status      - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL deinitialized succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_deinitialize(void *pHandle);

//*****************************************************************************
//
//! @brief Change the power state of the SYSPLL module.
//!
//! @param pHandle       - pointer to handle for the module instance.
//! @param ePowerState   - the desired power state of the SYSPLL.
//! @param bRetainState  - a flag to ask the HAL to save/restore SYSPLL
//!                        registers.
//! Note: bRetainState is not used in this API, as the PLL registers
//! will always retain its state regardless of PLL power state.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_power_control(void *pHandle, uint32_t ePowerState, bool bRetainState);

//*****************************************************************************
//
//! @brief System PLL enable function
//!
//! @param pHandle   - pointer to handle for the module instance.
//!
//! This function enables the system PLL operation.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL enabled succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//!         - AM_HAL_STATUS_INVALID_OPERATION: SIMOBUCK force no in place. Make
//!             sure am_bsp_low_power_init() is invoked before enabling system
//!             PLL.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_enable(void *pHandle);

//*****************************************************************************
//
//! @brief System PLL disable function
//!
//! @param pHandle   - pointer to handle for the module instance.
//!
//! This function disables the system PLL operation.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL disabled succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_disable(void *pHandle);

//*****************************************************************************
//
//! @brief System PLL configuration function
//!
//! @param pHandle   - pointer to handle for the module instance.
//! @param psConfig  - pointer to the configuration structure.
//!
//! This function configures the system PLL for operation.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL configured succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//!         - AM_HAL_STATUS_INVALID_OPERATION: Invalid Operation. Make sure
//!             that system PLL is not enabled.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid system PLL configuration. Make
//!             sure that config is valid.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_configure(void *pHandle, am_hal_syspll_config_t *psConfig);

//*****************************************************************************
//
//! @brief System PLL Get PLL Lock function
//!
//! @param pHandle   - pointer to handle for the module instance.
//! @param pbLocked     - pointer to boolean of PLL lock status.
//!
//! This function writes true to pbLocked when PLL clock is ready, and writes
//! false otherwise.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL lock status read succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_lock_read(void *pHandle, bool *pbLocked);

//*****************************************************************************
//
//! @brief System PLL Wait PLL Lock function
//!
//! @param pHandle   - pointer to handle for the module instance.
//!
//! This function waits for PLL lock according to System PLL settings
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL locked.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//!         - AM_HAL_STATUS_INVALID_OPERATION: System PLL not enabled
//!         - AM_HAL_STATUS_TIMEOUT: System PLL lock wait timed out
//
//*****************************************************************************
extern uint32_t am_hal_syspll_lock_wait(void *pHandle);

//*****************************************************************************
//
//! @brief System PLL refClk bypass function
//!
//! @param pHandle   - handle for the module instance.
//! @param bBypass   - boolean setting to bypass refClk.
//!
//! This function sets bypass setting to bypass PLL reference clock to its
//! output. This function is for debugging purposes only.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL bypass setting set succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_bypass_set(void *pHandle, bool bBypass);

//*****************************************************************************
//
//! @brief System PLL output divider configuration function
//!
//! @param pHandle   - handle for the module instance.
//! @param ui32OutId - output ID.
//! @param ui32Div   - divider value.
//!
//! This function configures the output divider for the system PLL.
//!
//! Note: For MEMPLL, ui32OutID is not used.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL output divider configured succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid ui32OutId or ui32Div.
//!         - AM_HAL_STATUS_OUT_OF_RANGE: ui32Div out of range.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_output_config(void *pHandle, uint32_t ui32OutId, uint32_t ui32Div);

//*****************************************************************************
//
//! @brief System PLL output enable function
//!
//! @param pHandle   - handle for the module instance.
//! @param ui32OutId - output ID.
//! @param bEnable   - boolean setting to release the output clock gate.
//!
//! This function enables or disables a System PLL output.
//! Note: For SYSPLL, only SYSCLK0 and SYSCLK3 can be directly enabled or
//!       disabled. The SYSCLK0 control also affects SYSCLK1 and SYSCLK2,
//!       and the SYSCLK3 control also affects SYSCLK4.
//!       Passing SYSCLK1, SYSCLK2, or SYSCLK4 returns an invalid argument
//!       error.
//!       For MEMPLL, ui32OutID is not used.
//!
//! @return status   - generic or interface specific status.
//!         - AM_HAL_STATUS_SUCCESS: System PLL output enabled succesfully.
//!         - AM_HAL_STATUS_INVALID_HANDLE: Invalid pHandle.
//!         - AM_HAL_STATUS_INVALID_ARG: Invalid ui32OutId or bEnable.
//!         - AM_HAL_STATUS_OUT_OF_RANGE: ui32OutId out of range.
//
//*****************************************************************************
extern uint32_t am_hal_syspll_output_enable(void *pHandle, uint32_t ui32OutId, bool bEnable);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SYSPLL_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
