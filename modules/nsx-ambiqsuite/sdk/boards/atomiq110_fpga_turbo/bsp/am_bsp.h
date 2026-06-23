//*****************************************************************************
//
//  am_bsp.h
//! @file
//!
//! @brief Functions to aid with configuring the GPIOs.
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
// This is part of revision stable-2026.06.18 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_BSP_H
#define AM_BSP_H

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"
#include "am_bsp_pins.h"
#include "am_devices_led.h"
#include "am_devices_button.h"
#include "am_devices_display_types.h"

#ifdef __cplusplus
extern "C"
{
#endif
#if defined (DISP_CTRL_IP)
//*****************************************************************************
//
// Board level display hardware configurations
//
//*****************************************************************************
extern am_devices_display_hw_config_t g_sDispCfg;
#endif // DISP_CTRL_IP

//*****************************************************************************
//
// Definition of External Clock Sources for the board
//
//*****************************************************************************
// #define AM_BSP_XTAL_HS_MODE         AM_HAL_CLKMGR_XTAL_HS_MODE_XTAL
// #define AM_BSP_XTAL_HS_FREQ_HZ      (48000000U)
// #define AM_BSP_XTAL_LS_MODE         AM_HAL_CLKMGR_XTAL_LS_MODE_XTAL
// #define AM_BSP_XTAL_LS_FREQ_HZ      (32768U)
// #define AM_BSP_EXTREF_CLK_FREQ_HZ   (0U)

//*****************************************************************************
//
// MSPI NAND FLASH definitions.
//
//*****************************************************************************
#define AM_BSP_MSPI_NAND_FLASH_DEVICE_NUM   1

#define AM_BSP_MSPI_NAND_FLASH_U31
#define AM_BSP_MSPI_NAND_FLASH_U31_DESIGNATOR   31
#define AM_BSP_MSPI_NAND_FLASH_U31_MODULE       0

typedef struct
{
    uint32_t    ui32Designator;
    uint32_t    ui32Module;
}am_bsp_nand_flash_t;

extern const am_bsp_nand_flash_t bsp_nand_flash_devices[];

// #### INTERNAL BEGIN ####
//
// Define MCU_VALIDATION for building tests for the validation group.
//
// Test Result/Progress
#define MCU_VALIDATION_DEBUG_REG    0x4FFFF000 // TB Debug Register

// Test GPIO used in `am_tuil_test.h`
// If defined, test will keep toggling the pin to indicate progress
// Need to select an unused GPIO in the BSP.
// #define MCU_VALIDATION_GPIO

extern const am_hal_gpio_pincfg_t g_AM_VALIDATION_GPIO;
// #### INTERNAL END ####

//*****************************************************************************
//
// UART specifics for this board including assigned UART modules.
//
//*****************************************************************************
#define AM_BSP_UART_PRINT_INST  0   // UART COM module

//*****************************************************************************
//
// PWM_LED peripheral assignments.
//
//*****************************************************************************
#define AM_BSP_PWM_LED_TIMER                0
#define AM_BSP_PWM_LED_TIMER_SEG            AM_HAL_CTIMER_TIMERB
#define AM_BSP_PWM_LED_TIMER_INT            AM_HAL_CTIMER_INT_TIMERB0

//*****************************************************************************
//
// Button definitions.
//
//*****************************************************************************
#define AM_BSP_NUM_BUTTONS                  0
#if AM_BSP_NUM_BUTTONS
extern am_devices_button_t am_bsp_psButtons[AM_BSP_NUM_BUTTONS];
#endif

//*****************************************************************************
//
// LED definitions.
//
//*****************************************************************************
#if 1   // FPGAs and EBs generally have no native LEDs.
#undef  AM_BSP_NUM_LEDS
#else
#define AM_BSP_NUM_LEDS                     4
extern am_devices_led_t am_bsp_psLEDs[AM_BSP_NUM_LEDS];
#endif

//*****************************************************************************
//
// MSPI FLASH definitions.
//
//*****************************************************************************
#define AM_BSP_MSPI_FLASH_MODULE            2
#define AM_BSP_MSPI_FLASH_DEVICE_IS25WX064
//#define AM_BSP_MSPI_FLASH_DEVICE_ATXP032

//*****************************************************************************
//
// Touch interface definitions.
//
//*****************************************************************************
#define AM_BSP_TP_IOM_MODULE                 6

//*****************************************************************************
//
// MSPI PSRAM definitions.
//
//*****************************************************************************
#define AM_BSP_MSPI_PSRAM_DEVICE_APS25616N
#define AM_BSP_MSPI_PSRAM_MODULE_OCTAL_DDR_CE   AM_HAL_MSPI_FLASH_OCTAL_DDR_CE0

//*****************************************************************************
//
// IOM PSRAM definitions.
//
//*****************************************************************************
#define AM_BSP_IOM_PSRAM_MODULE            2

//*****************************************************************************
//
// IOM - IOS macro definitions for ios 2-board testing
//
//*****************************************************************************

//
// ios_device
//
#define TEST_IOS_MODULE                 0
#define SLINT_GPIO                      0
#define lram_array                      am_hal_iosfd0_pui8LRAM
#define IOS_ACC_IRQ                     IOSLAVEFDACC0_IRQn
#define IOS_IRQ                         IOSLAVEFD0_IRQn
#define ios_isr                         am_ioslave_fd0_isr
#define ios_acc_isr                     am_ioslave_fd0_acc_isr
#define SLINT_FUN_SEL                   AM_HAL_PIN_4_SLFDINT
#define IOS_PWRCTRL                     AM_HAL_PWRCTRL_PERIPH_IOSFD0
#define IOS_WRAPAROUND_MAX_LRAM_SIZE    0x40
#define IOS_FIFO_THRESHOLD              0x20

//
// ios_controller
//
#define TEST_IOS_CNTLR_MODULE           1
#define am_iomaster_isr                 am_iomaster1_isr
#define IOMSTR_IQRn                     IOMSTR1_IRQn

//
// ios_fullduplex
//
#define IOM_DUPLEX_TEST_MODULE          1

//
// ios_device and ios_controller pins
//
#define AM_BSP_IOS_TEST_IOM_SCK         AM_BSP_GPIO_IOM1_SCK
#define AM_BSP_IOS_TEST_IOM_MOSI        AM_BSP_GPIO_IOM1_MOSI
#define AM_BSP_IOS_TEST_IOM_MISO        AM_BSP_GPIO_IOM1_MISO
#define AM_BSP_IOS_TEST_IOM_CS          AM_BSP_GPIO_IOM1_CS


#define AM_BSP_IOS_TEST_IOS_SCK         AM_BSP_GPIO_IOSFD0_SCK
#define AM_BSP_IOS_TEST_IOS_MOSI        AM_BSP_GPIO_IOSFD0_MOSI
#define AM_BSP_IOS_TEST_IOS_MISO        AM_BSP_GPIO_IOSFD0_MISO
#define AM_BSP_IOS_TEST_IOS_CE          AM_BSP_GPIO_IOSFD0_CE

// #### INTERNAL BEGIN ####
// iom handshake pin for loopback test
#define IOM_HANDSHAKE_PIN               46
#define IOM_HANDSHAKE_PIN_IRQ           GPIO0_203F_IRQn
#define IOM_HANDSHAKE_PIN_ISR           am_gpio0_203f_isr
#define IOS_HANDSHAKE_PIN               AM_BSP_GPIO_IOSFD0_INT
#define IOS_HANDSHAKE_PINCFG            g_AM_BSP_GPIO_IOSFD0_INT
// #### INTERNAL END ####
//*****************************************************************************
//
// MSPI Display definitions.
//
//*****************************************************************************
#define AM_BSP_MSPI_CLKOND4(inst)  \
    ((inst == g_sDispCfg.ui32Module)?g_sDispCfg.bClockonD4:false)

//*****************************************************************************
//
// JDI timer pins definitions.
//
//*****************************************************************************
#define AM_BSP_JDI_TIMER_VA               0
#define AM_BSP_JDI_TIMER_VCOM             1
#define AM_BSP_JDI_TIMER_VB               2
// #### INTERNAL BEGIN ####
//*****************************************************************************
//
// Memory for a faked printf application
//
//*****************************************************************************
typedef struct
{
    uint8_t *pui8Buffer;
    uint32_t ui32Size;
    uint32_t ui32Index;
} am_bsp_memory_printf_state_t;
// #### INTERNAL END ####

//*****************************************************************************
//
// SDIO Bus Width definitions
//
//*****************************************************************************
#define AM_BSP_SDIO_INSTANCE    (0)
#define AM_BSP_SDIO_BUS_WIDTH   AM_HAL_HOST_BUS_WIDTH_8

//*****************************************************************************
//
// Assigned to reversed pins to disable SDIO CD & WP.
//
//*****************************************************************************
#define AM_BSP_GPIO_SDIO0_WP                12
#define AM_BSP_GPIO_SDIO0_CD                13

#define AM_BSP_GPIO_SDIO1_WP                40
#define AM_BSP_GPIO_SDIO1_CD                41

//*****************************************************************************
//
// External function definitions.
//
//*****************************************************************************

//*****************************************************************************
//
//! @brief Set display reset pins.
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_disp_reset_pins_set(void);

//*****************************************************************************
//
//! @brief Clear display reset pins.
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_disp_reset_pins_clear(void);

//*****************************************************************************
//
//! @brief Set up the display pins.
//!
//! This function configures reset,te,en,mode selection and clock/data pins
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_disp_pins_enable(void);

//*****************************************************************************
//
//! @brief Deinit the display pins.
//!
//! This function de-configures reset,te,en,mode selection and clock/data pins
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_disp_pins_disable(void);

//*****************************************************************************
//
//! @brief get jdi timer pins.
//!
//! This function could get pins number of VA,VCOM,VB
//!
//! @return None.
//
//*****************************************************************************
extern uint32_t am_bsp_disp_jdi_timer_pins(uint8_t ui8TimerPin);

//*****************************************************************************
//
//! @brief Set up the I2S pins based on module.
//!
//! @param ui32Module - I2S module
//! @param bBidirectionalData - Use Bidirectional Data for I2S Module
//
//*****************************************************************************
extern void am_bsp_i2s_pins_enable(uint32_t ui32Module, bool bBidirectionalData);

//*****************************************************************************
//
//! @brief Disable the I2S pins based on module.
//!
//! @param ui32Module - I2S module
//! @param bBidirectionalData - Use Bidirectional Data for I2S Module
//
//*****************************************************************************
extern void am_bsp_i2s_pins_disable(uint32_t ui32Module, bool bBidirectionalData);

//*****************************************************************************
//
//! @brief Set up the PDM pins based on module.
//!
//! @param ui32Module - PDM module
//
//*****************************************************************************
extern void am_bsp_pdm_pins_enable(uint32_t ui32Module);

//*****************************************************************************
//
//! @brief Disable the PDM pins based on module.
//!
//! @param ui32Module - PDM module
//
//*****************************************************************************
extern void am_bsp_pdm_pins_disable(uint32_t ui32Module);

extern void am_bsp_low_power_init(void);
extern void am_bsp_iom_pins_enable(uint32_t ui32Module, am_hal_iom_mode_e eIOMMode);
extern void am_bsp_iom_pins_disable(uint32_t ui32Module, am_hal_iom_mode_e eIOMMode);
extern void am_bsp_iom_3wire_pins_enable(uint32_t ui32Module, am_hal_iom_mode_e eIOMMode);
extern void am_bsp_iom_3wire_pins_disable(uint32_t ui32Module, am_hal_iom_mode_e eIOMMode);
extern void am_bsp_ios_pins_enable(uint32_t ui32Module, uint32_t ui32IOSMode);
extern void am_bsp_ios_pins_disable(uint32_t ui32Module, uint32_t ui32IOSMode);


//*****************************************************************************
//
//! @brief Enable the TPIU and ITM for debug printf messages.
//!
//! This function enables TPIU registers for debug printf messages and enables
//! ITM GPIO pin to SWO mode. This function should be called after reset and
//! after waking up from deep sleep.
//!
//! @return 0 on success.
//
//*****************************************************************************
extern int32_t am_bsp_debug_printf_enable(void);

//*****************************************************************************
//
//! @brief Disable the TPIU and ITM for debug printf messages.
//!
//! This function disables TPIU registers for debug printf messages and disables
//! ITM GPIO pin to SWO mode. This function should be called after reset and
//! after waking up from deep sleep.
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_debug_printf_disable(void);

//*****************************************************************************
//
//! @brief Prepare for deepsleep while keeping DEBUG active.
//!
//!  When an application finds it necessary to not disable printing before
//!  deepsleep, this function should be called just before and just after
//!  deepsleep to make sure the SWO line is properly handled.
//!
//!  Important - This function is only needed if the application needs to
//!  keep SWO/ITM enabled during deepsleep. It is not needed when the
//!  print protocol to disable before deepsleep is used.
//!
//!  However, under these conditions, the deepsleep level will only reach the
//!  CORE_DEEPSLEEP level rather than the SYS_DEEPSLEEP level (see also
//!  MCUCTRL->SYSPWRSTATUS).
//
//*****************************************************************************
extern void am_bsp_debug_printf_deepsleep_prepare(bool bGoingToSleep);

//*****************************************************************************
//
//! @brief Enable printing over ITM.
//
//! @return 0 on success.
//
//*****************************************************************************
extern int32_t am_bsp_itm_printf_enable(void);

//*****************************************************************************
//
// @brief Disable printing over ITM.
//
//*****************************************************************************
extern void am_bsp_itm_printf_disable(void);

//*****************************************************************************
//
//! Initialize and configure the UART
//
//! @return 0 on success.
//
//*****************************************************************************
extern int32_t am_bsp_uart_printf_enable(void);

//*****************************************************************************
//
//! Disable the UART
//
//*****************************************************************************
extern void am_bsp_uart_printf_disable(void);

//*****************************************************************************
//
//! @brief UART-based string print function.
//!
//! @param pcString - Pointer to character array to print
//!
//! This function is used for printing a string via the UART, which for some
//! MCU devices may be multi-module.
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_uart_string_print(char *pcString);

#ifndef AM_BSP_DISABLE_BUFFERED_UART
//*****************************************************************************
//
//! Initialize and configure the UART
//!
//! @param pvHandle - Pointer to UART Handle
//!
//! @return 0 on success.
//
//*****************************************************************************
extern int32_t am_bsp_buffered_uart_printf_enable(void *pvHandle);

//*****************************************************************************
//
//! Disable the UART
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_buffered_uart_printf_disable(void);

//*****************************************************************************
//
//! Interrupt routine for the buffered UART interface.
//
//*****************************************************************************
extern void am_bsp_buffered_uart_service(void);
#endif // AM_BSP_DISABLE_BUFFERED_UART

// #### INTERNAL BEGIN ####
extern int32_t am_bsp_memory_printf_enable(am_bsp_memory_printf_state_t *psPrintfState);
extern void am_bsp_memory_printf_disable(void);
// #### INTERNAL END ####
#ifdef __cplusplus
}
#endif

//*****************************************************************************
//
//! @brief Put the test errors to the TPIU and ITM.
//!
//! @param i32ErrorCount
//!
//! This function outputs the number of test errors to the TPIU/ITM.
//!
//! @return None.
//
//*****************************************************************************
extern void am_bsp_ckerr(int32_t i32ErrorCount);

//*****************************************************************************
//
//! @brief Set up the SDIO pins based on module and bus width.
//!
//! @param ui8SdioNum - SDIO/eMMC module number.
//! @param ui8BusWidth - SDIO/eMMC bus width.
//
//*****************************************************************************
extern void am_bsp_sdio_pins_enable(uint8_t ui8SdioNum, uint8_t ui8BusWidth);

//*****************************************************************************
//
//! @brief Reset the SDIO peripheral interface for a module.
//!
//! @param ui32Module - SDIO/eMMC module number.
//
//*****************************************************************************
extern void am_bsp_sdio_reset(uint32_t ui32Module);
#endif // AM_BSP_H
//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
