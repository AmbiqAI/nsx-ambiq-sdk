//*****************************************************************************
//
//! @file am_reg.h
//!
//! @brief Atomiq110 register macros
//!
//! @addtogroup reg_macros_5 Apollo6 Register Macros
//! @ingroup apollo6_hal
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
#ifndef AM_REG_H
#define AM_REG_H

//*****************************************************************************
//
//! @brief SYSPLL
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SYSPLL_NUM_MODULES                       1

//*****************************************************************************
//
//! @brief CLKGEN
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_CLKGEN_NUM_MODULES                    1
#define AM_REG_CLKGENn(n)   \
        (CLKGEN_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief RSTGEN
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_RSTGEN_NUM_MODULES                    1
#define AM_REG_RSTGENn(n)   \
        (RSTGEN_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SECURITY
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SECURITY_NUM_MODULES                  1
#define AM_REG_SECURITYn(n)   \
        (SECURITY_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief CRM
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_CRM_NUM_MODULES                       1
#define AM_REG_CRMn(n)   \
        (CRM_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief RTC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_RTC_NUM_MODULES                       1
#define AM_REG_RTCn(n)   \
        (RTC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SECURERTC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SECURERTC_NUM_MODULES                 1
#define AM_REG_SECURERTCn(n)   \
        (SECURERTC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SECUREWDT
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SECUREWDT_NUM_MODULES                 1
#define AM_REG_SECUREWDTn(n)   \
        (SECUREWDT_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief WDT
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_WDT_NUM_MODULES                       1
#define AM_REG_WDTn(n)   \
        (WDT_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief EMMC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_EMMC_NUM_MODULES                      1
#define AM_REG_EMMCn(n)   \
        (EMMC_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief I3C
//! Instance finder. (3 instance(s) available)
//
//*****************************************************************************
#define AM_REG_I3C_NUM_MODULES                       3
#define AM_REG_I3Cn(n)   \
        (I3C_BASE + 0x00003000 * n)

//*****************************************************************************
//
//! @brief USB
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_USB_NUM_MODULES                       1
#define AM_REG_USBn(n)   \
        (USB_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief USBPHY
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_USBPHY_NUM_MODULES                    1
#define AM_REG_USBPHYn(n)   \
        (USBPHY_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief AXI
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_AXI_NUM_MODULES                       1
#define AM_REG_AXIn(n)   \
        (AXI_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief MMU
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_MMU_NUM_MODULES                       1
#define AM_REG_MMUn(n)   \
        (MMU_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief CRYPTO
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_CRYPTO_NUM_MODULES                    1
#define AM_REG_CRYPTOn(n)   \
        (CRYPTO_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief DMEA
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_DMEA_NUM_MODULES                      1
#define AM_REG_DMEAn(n)   \
        (DMEA_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief IOM
//! Instance finder. (12 instance(s) available)
//
//*****************************************************************************
#define AM_REG_IOM_NUM_MODULES                       12
#define AM_REG_IOMn(n)   \
        (IOM0_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief IOSLAVEFD
//! Instance finder. (2 instance(s) available)
//
//*****************************************************************************
#define AM_REG_IOSLAVEFD_NUM_MODULES                 2
#define AM_REG_IOSLAVEFDn(n)   \
        (IOSLAVEFD0_BASE + 0x00000400 * n)

//*****************************************************************************
//
//! @brief MCUCTRL
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_MCUCTRL_NUM_MODULES                   1
#define AM_REG_MCUCTRLn(n)   \
        (MCUCTRL_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief DC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_DC_NUM_MODULES                        1
#define AM_REG_DCn(n)   \
        (DC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief DSI
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_DSI_NUM_MODULES                       1
#define AM_REG_DSIn(n)   \
        (DSI_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief GPU
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_GPU_NUM_MODULES                       1
#define AM_REG_GPUn(n)   \
        (GPU_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief RRAM
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_RRAM_NUM_MODULES                      1
#define AM_REG_RRAMn(n)   \
        (RRAM_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief NPU
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_NPU_NUM_MODULES                       1
#define AM_REG_NPUn(n)   \
        (NPU_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SDIO
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SDIO_NUM_MODULES                      1
#define AM_REG_SDIOn(n)   \
        (SDIO0_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief SCM
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SCM_NUM_MODULES                       1
#define AM_REG_SCMn(n)   \
        (SCM_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief UART
//! Instance finder. (6 instance(s) available)
//
//*****************************************************************************
#define AM_REG_UART_NUM_MODULES                      6
#define AM_REG_UARTn(n)   \
        (UART0_BASE + 0x00001000 * n)

//*****************************************************************************
//
//! @brief VCOMP
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_VCOMP_NUM_MODULES                     1
#define AM_REG_VCOMPn(n)   \
        (VCOMP_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief ADC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_ADC_NUM_MODULES                       1
#define AM_REG_ADCn(n)   \
        (ADC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief LPADC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_LPADC_NUM_MODULES                     1
#define AM_REG_LPADCn(n)   \
        (LPADC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief FPIO
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_FPIO_NUM_MODULES                      1
#define AM_REG_FPIOn(n)   \
        (FPIO_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief GPIO
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_GPIO_NUM_MODULES                      1
#define AM_REG_GPIOn(n)   \
        (GPIO_BASE + 0x00000004 * n)

//*****************************************************************************
//
//! @brief XSPIMC
//! Instance finder. (3 instance(s) available)
//
//*****************************************************************************
#define AM_REG_XSPIMC_NUM_MODULES                    3
#define AM_REG_XSPIMCn(n)   \
        (XSPIMC0_BASE + 0x00004000 * n)

//*****************************************************************************
//
//! @brief XSPIS
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_XSPIS_NUM_MODULES                     1
#define AM_REG_XSPISn(n)   \
        (XSPIS_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief XSPISLV
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_XSPISLV_NUM_MODULES                   1
#define AM_REG_XSPISLVn(n)   \
        (XSPISLV_BASE + 0x00000400 * n)

//*****************************************************************************
//
//! @brief PWRCTRL
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_PWRCTRL_NUM_MODULES                   1
#define AM_REG_PWRCTRLn(n)   \
        (PWRCTRL_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SSC
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SSC_NUM_MODULES                       1
#define AM_REG_SSCn(n)   \
        (SSC_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief STIMER
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_STIMER_NUM_MODULES                    1
#define AM_REG_STIMERn(n)   \
        (STIMER_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief TIMER
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_TIMER_NUM_MODULES                     1
#define AM_REG_TIMERn(n)   \
        (TIMER_BASE + 0x00000020 * n)

//*****************************************************************************
//
//! @brief SECURESTIMER
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SECURESTIMER_NUM_MODULES              1
#define AM_REG_SECURESTIMERn(n)   \
        (SECURESTIMER_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief SECURETIMER
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_SECURETIMER_NUM_MODULES               1
#define AM_REG_SECURETIMERn(n)   \
        (SECURETIMER_BASE + 0x00000020 * n)

//*****************************************************************************
//
//! @brief I2S
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_I2S_NUM_MODULES                       1
#define AM_REG_I2Sn(n)   \
        (I2S0_BASE + 0x00000000 * n)

//*****************************************************************************
//
//! @brief PDM
//! Instance finder. (3 instance(s) available)
//
//*****************************************************************************
#define AM_REG_PDM_NUM_MODULES                       3
#define AM_REG_PDMn(n)   \
        (PDM0_BASE + 0x00000400 * n)

//*****************************************************************************
//
//! @brief OTP
//! Instance finder. (1 instance(s) available)
//
//*****************************************************************************
#define AM_REG_OTP_NUM_MODULES                       1
#define AM_REG_OTPn(n)   \
        (OTP_BASE + 0x00000000 * n)

#endif // AM_REG_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
