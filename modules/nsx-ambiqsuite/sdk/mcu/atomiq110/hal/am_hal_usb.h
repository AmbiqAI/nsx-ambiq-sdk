//*****************************************************************************
//
//! @file am_hal_usb.h
//!
//! @brief Functions for USB module
//!
//! @addtogroup usb_atomiq110 USB Functionality
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

#ifndef AM_HAL_USB_H_
#define AM_HAL_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define USBn(n) ((USB_Type*)(USB_BASE + (n * (USB_BASE - USB_BASE))))

//
//! USB PHY Clock frequency
//
#define AM_HAL_USB_PHY_CLK_FREQ_HZ (24000000)

//*****************************************************************************
//
//! @brief Enum for the USB speed type.
//!
//! These macros correspond to the defintions in the USB specification.
//! They may be used with the \e am_hal_usb_set_dev_speed() function.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_SPEED_LOW,
    AM_HAL_USB_SPEED_FULL,
    AM_HAL_USB_SPEED_HIGH,
    AM_HAL_USB_SPEED_UNKNOWN
}
am_hal_usb_dev_speed_e;

//*****************************************************************************
//
//! @brief Enum for the USB device state.
//!
//! These macros partially correspond to the defintions in the USB specification.
//! They may be used with the \e am_hal_usb_set_dev_state() function.
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_DEV_STATE_INIT,
    AM_HAL_USB_DEV_STATE_ADDRESSED,
    AM_HAL_USB_DEV_STATE_CONFIGED,
    AM_HAL_USB_DEV_STATE_RESUMING,
    AM_HAL_USB_DEV_STATE_ACTIVE,
    AM_HAL_USB_DEV_STATE_SUSPENDING,
    AM_HAL_USB_DEV_STATE_SUSPENDED,
    // #### INTERNAL BEGIN ####
    AM_HAL_USB_DEV_STATE_SLEPT,
    // #### INTERNAL END ####
}
am_hal_usb_dev_state_e;

//*****************************************************************************
//
//! @brief Enum for the USB device bus event.
//!
//! These macros correspond to the defintions in the USB specification.
//! They may be used with the \e am_hal_usb_dev_evt_callback callback function
//! provided by upper layer USB stack.
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_DEV_EVT_BUS_RESET,
    AM_HAL_USB_DEV_EVT_SOF,
    AM_HAL_USB_DEV_EVT_RESUME,
    AM_HAL_USB_DEV_EVT_SUSPEND,
    // #### INTERNAL BEGIN ####
    AM_HAL_USB_DEV_EVT_SLEEP,
    // #### INTERNAL END ####
    AM_HAL_USB_DEV_EVT_NUM
}
am_hal_usb_dev_event_e;

//*****************************************************************************
//
//! @brief Enum for the USB transfer status.
//!
//! These macros is used to indicate how the USB transfer is completed.
//! They may be used with the \e am_hal_usb_ep_xfer_complete_callback callback
//! function provided by upper layer USB stack.
//!
//
//*****************************************************************************
typedef enum
{
    // Transfer is done without error, for ctrl it means status packet done
    USB_XFER_DONE,
    // For control transfer only, data stage is done without error
    USB_XFER_DATA,
    // Endpoint stall is set
    USB_XFER_STALL,
    // Endpoint stall is cleared
    USB_XFER_UNSTALL,
    // Transfer is aborted
    USB_XFER_ABORT,
    // Transfer is aborted because endpoint reset/disable
    USB_XFER_RESET,
    // There was an error
    USB_XFER_ERROR
}
am_hal_usb_xfer_code_e;

//*****************************************************************************
//
//! @brief Enum for the USB Xfer Mode Setting
//!
//! The macros defined here are used to specify USB Xfer Mode Setting
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_OUT_PIO_MODE,
    AM_HAL_USB_OUT_DMA_MODE,
    AM_HAL_USB_IN_PIO_MODE,
    AM_HAL_USB_IN_DMA_MODE,
} am_hal_usb_xfer_mode_e;

//*****************************************************************************
//
//! @brief Enum for the USB Xfer Direction
//!
//! The macros defined here are used to specify USB Xfer Direction
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_OUT_DIR = 0,
    AM_HAL_USB_IN_DIR = 1
} am_hal_usb_xfer_dir_e;

//*****************************************************************************
//
//! @brief Packet-buffer mode allocated to a non-control endpoint.
//!
//! Values map directly to the USB controller's CONBUF register:
//! 00=single, 01=double, 10=triple, 11=quad.
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_EP_BUFFER_COUNT_SINGLE = 0,
    AM_HAL_USB_EP_BUFFER_COUNT_DOUBLE = 1,
    AM_HAL_USB_EP_BUFFER_COUNT_TRIPLE = 2,
    AM_HAL_USB_EP_BUFFER_COUNT_QUAD   = 3,
} am_hal_usb_ep_buffer_count_e;

//*****************************************************************************
//
//! @brief Enum for the USB PHY reference clock source
//!
//! The macros defined here are used to specify USB PHY reference clock source
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_PHYCLKSRC_RF_XTAL_48M    = CRM_USBPHYCORECRM_USBPHYCORECLKSEL_XTAL_HS,
    AM_HAL_USB_PHYCLKSRC_EXTREF_CLK     = CRM_USBPHYCORECRM_USBPHYCORECLKSEL_EXTREFCLK,
    AM_HAL_USB_PHYCLKSRC_HFRC_125M      = CRM_USBPHYCORECRM_USBPHYCORECLKSEL_HFRC_div2_div2,
    AM_HAL_USB_PHYCLKSRC_SYS_CLK0       = CRM_USBPHYCORECRM_USBPHYCORECLKSEL_SYS_CLK0,
    AM_HAL_USB_PHYCLKSRC_SYS_CLK3       = CRM_USBPHYCORECRM_USBPHYCORECLKSEL_SYS_CLK3,
    AM_HAL_USB_PHYCLKSRC_DEFAULT        = AM_HAL_USB_PHYCLKSRC_SYS_CLK3 + 1,
} am_hal_usb_phyclksrc_e;

//*****************************************************************************
//
//! @brief Enum for the USB PHY reference clock source divider ratio
//!
//! The macros defined here are used to specify USB PHY reference clock source
//! divider ratio
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_PHYCLKSRC_DIV_1 = 0,
    AM_HAL_USB_PHYCLKSRC_DIV_2 = 1,
    AM_HAL_USB_PHYCLKSRC_DIV_3 = 2,
    AM_HAL_USB_PHYCLKSRC_DIV_4 = 3,
} am_hal_usb_phyclksrc_div_e;

//*****************************************************************************
//
//! @brief device event callback function pointer type
//!
//! Upper layer USB stack uses it to define a callback function to receive the
//! the USB bus events and registers it by 'am_hal_usb_register_dev_evt_callback'
//! function.
//
//*****************************************************************************
typedef void (*am_hal_usb_dev_evt_callback)(am_hal_usb_dev_event_e eDevState);

//*****************************************************************************
//
//! @brief ep0 setup request callback function pointer type
//!
//! Upper layer USB stack uses it to define a callback function to receive
//! setup requst from the USB host and registers it by
//! 'am_hal_usb_register_ep0_setup_received_callback' function.
//
//*****************************************************************************
typedef void (*am_hal_usb_ep0_setup_received_callback)(uint8_t *pui8Setup);

//*****************************************************************************
//
//! @brief ctrl/bulk/intr/iso transfer callback function pointer type
//!
//! Upper layer USB stack uses it to define a callback function to receive a
//! confirm for the USB HAL driver about how many bytes has been tranfered and
//! the transfer status.
//
//*****************************************************************************
typedef void (*am_hal_usb_ep_xfer_complete_callback)(uint8_t ui8EpAddr, uint32_t ui32XferLen, am_hal_usb_xfer_code_e eXferCode, void *param);

//*****************************************************************************
//
//! @brief register a USB bus event callback function
//!
//! @param pHandle - handle for the module instance.
//! @param cb      - a USB bus event callback function.
//!
//! This function registers a USB bus event callback function defined in the upper layer
//! USB stack.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_register_dev_evt_callback(void *pHandle, am_hal_usb_dev_evt_callback cb);

//*****************************************************************************
//
//! @brief register a setup requst callback function
//!
//! @param pHandle - handle for the module instance.
//! @param cb      - setup request callback function.
//!
//! This function registers a setup request callback function defined in the upper layer
//! USB stack.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_register_ep0_setup_received_callback(void *pHandle, am_hal_usb_ep0_setup_received_callback cb);

//*****************************************************************************
//
//! @brief register a transfer completion callback function
//!
//! @param pHandle - handle for the module instance.
//! @param cb      - transfer completion callback function.
//!
//! This function registers a bulk/intr/iso transfer completion callback function.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_register_ep_xfer_complete_callback(void *pHandle, am_hal_usb_ep_xfer_complete_callback cb);

//*****************************************************************************
//
//! @brief set the USB device state
//!
//! @param pHandle   - handle for the module instance.
//! @param eDevState - device states like suspended, resuming, etc.
//!
//! This function is used by upper layer USB stack to set the device state.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_dev_state(void *pHandle, am_hal_usb_dev_state_e eDevState);

//*****************************************************************************
//
//! @brief Complete L2 suspend: gate controller clocks if still SUSPENDING.
//!
//! SUSPENDING is set in am_hal_usb_interrupt_service() on suspir before the
//! suspend callback runs.
//
//*****************************************************************************
extern uint32_t am_hal_usb_complete_l2_suspend(void *pHandle);

//*****************************************************************************
//
//! @brief Complete L2 resume: set device state to ACTIVE.
//
//*****************************************************************************
extern uint32_t am_hal_usb_complete_l2_resume(void *pHandle);

// #### INTERNAL BEGIN ####
// Note: Remote wakeup feature is declared as unsupported as mentioned in
//       [CAR-3940]
//
//*****************************************************************************
//
//! @brief USB remote wakeup HAL support control
//!
//! Defaults to disabled (0). When disabled (the default), both
//! am_hal_usb_start_remote_wakeup() and am_hal_usb_end_remote_wakeup()
//! always return AM_HAL_STATUS_INVALID_OPERATION without touching hardware.
//! The register-level implementation is retained for Ambiq-internal
//! bring-up/validation and is only compiled in when this is set to 1 by the
//! build; it is stripped from public source packages.
//
//*****************************************************************************
#ifndef AM_HAL_USB_REMOTE_WAKEUP_SUPPORT
#define AM_HAL_USB_REMOTE_WAKEUP_SUPPORT 0
#endif
// #### INTERNAL END ####

//*****************************************************************************
//
//! @brief start the remote wakeup
//!
//! @param pHandle - handle for the module instance.
//!
//! Not supported. Always returns AM_HAL_STATUS_INVALID_OPERATION.
//!
//! @return AM_HAL_STATUS_INVALID_OPERATION
//
//*****************************************************************************
extern uint32_t am_hal_usb_start_remote_wakeup(void *pHandle);

//*****************************************************************************
//
//! @brief end the remote wakeup
//!
//! @param pHandle - handle for the module instance.
//!
//! Not supported. Always returns AM_HAL_STATUS_INVALID_OPERATION.
//!
//! @return AM_HAL_STATUS_INVALID_OPERATION
//
//*****************************************************************************
extern uint32_t am_hal_usb_end_remote_wakeup(void *pHandle);

// #### INTERNAL BEGIN ####
//*****************************************************************************
//
//! @brief LPM L1 support control
//
//*****************************************************************************
#ifndef AM_HAL_USB_LPM_L1_SUPPORT
#define AM_HAL_USB_LPM_L1_SUPPORT 1
#endif

#if AM_HAL_USB_LPM_L1_SUPPORT
//*****************************************************************************
//
//! @brief Enable L1 Sleep
//!
//! @param pHandle - handle for the module instance.
//!
//! Sets sleep_entry=1 (auto UTMI gate on L1 entry), lpmnyet=0 (ACK), and
//! enables the L1 IRQ. Called once after bus reset from the DCD.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_l1_enable(void *pHandle);

//*****************************************************************************
//
//! @brief Disable L1 Sleep
//!
//! @param pHandle - handle for the module instance.
//!
//! Sets lpmnyet=1 (NYET / refuse L1 tokens), clears sleep_entry, and masks
//! the L1 IRQ. Pair with am_hal_usb_l1_enable() to restore ACK / L1 entry.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_l1_disable(void *pHandle);

//*****************************************************************************
//
//! @brief read LPM-token bRemoteWake from hardware
//!
//! @param pHandle - handle for the module instance.
//!
//! Safe to call after a SLEEP event (lpmir already cleared in
//! am_hal_usb_intr_status_get). Upper layer copies the result for policy;
//! this value is not retained in HAL state.
//!
//! @return true if the latest LPM token granted device remote wake
//
//*****************************************************************************
extern bool am_hal_usb_l1_remote_wakeup_get(void *pHandle);
#endif
// #### INTERNAL END ####

//*****************************************************************************
//
//! @brief set the USB device address
//!
//! @param pHandle   - handle for the module instance.
//! @param ui8EpAddr - set the USB device address
//!
//! This function is used by upper layer USB stack to set the device address allocated
//! by the USB host during the enumeration.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_addr(void *pHandle, uint8_t ui8EpAddr);

//*****************************************************************************
//
//! @brief soft connect to the USB host
//!
//! @param pHandle - handle for the module instance.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_attach(void *pHandle);

//*****************************************************************************
//
//! @brief soft disconnect to the USB host
//!
//! @param pHandle - handle for the module instance.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_detach(void *pHandle);

//*****************************************************************************
//
//! @brief get the USB frame number
//!
//! @param pHandle - handle for the module instance.
//!
//! This function is used by upper layer USB stack to get the USB frame number.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_get_frame_number(void *pHandle);

//*****************************************************************************
//
//! @brief enable the SOF interrupt
//!
//! @param pHandle - handle for the module instance.
//!
//! This function is used by upper layer USB stack to enable the SOF interrupt.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_enable_sof_intr(void *pHandle);

//*****************************************************************************
//
//! @brief disable the SOF interrupt
//!
//! @param pHandle   - handle for the module instance.
//!
//! This function is used by upper layer USB stack to disable the SOF interrupt.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_disable_sof_intr(void *pHandle);

#define AM_HAL_USB_GET_HW_INFO_ENABLED
#ifdef AM_HAL_USB_GET_HW_INFO_ENABLED

//*****************************************************************************
//
//! @brief Macro definitions for the USB hardware information.
//!
//! These macros correspond to the definitions in the datasheet of
//! Apollo510L USB peripheral.
//! They may be used with the \e am_hal_usb_get_hw_infor function.
//!
//
//*****************************************************************************
typedef struct
{
    // the major version number
    uint8_t  ui8Major;
    // the minor version number
    uint16_t ui16Minor;
    // the number of OUT endpoints
    uint8_t  ui8OutEpNum;
    // the number of IN endpoints
    uint8_t  ui8InEpNum;
    // the width of RAM bus address
    uint8_t  ui8RamBits;
} am_hal_usb_hw_info;

//*****************************************************************************
//
//! @brief get the hardware information
//!
//! @param pHandle - handle for the module instance.
//! @param sHWInfo - the information about the USB device hardware configuration.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_get_hw_infor(void *pHandle, am_hal_usb_hw_info *sHWInfo);

#endif

#define AM_HAL_USB_TEST_MODE_ENABLED
#ifdef AM_HAL_USB_TEST_MODE_ENABLED

//*****************************************************************************
//
//! @brief Macro definitions for the USB devie test mode.
//!
//! These macros correspond to the defintions in the USB specification.
//! They may be used with the \e am_hal_usb_test_mode function.
//!
//
//*****************************************************************************
typedef enum
{
    AM_HAL_USB_TEST_SE0_NAK,
    AM_HAL_USB_TEST_J,
    AM_HAL_USB_TEST_K,
    AM_HAL_USB_TEST_PACKET
}
am_hal_usb_test_mode_e;

//
// Upper layer USB stack should call below functions
// when receive TEST_MODE request
//
//*****************************************************************************
//
//! @brief set the USB test mode flag
//!
//! @param pHandle - handle for the module instance.
//!
//! USB test mode is not supported on this device.
//!
//! @return AM_HAL_STATUS_INVALID_OPERATION
//
//*****************************************************************************
extern uint32_t am_hal_usb_enter_test_mode(const void *pHandle);

//*****************************************************************************
//
//! @brief do the USB test
//!
//! @param pHandle   - handle for the module instance.
//! @param eTestMode - one of the test mode like TEST_SE0_NAK, TEST_J, TEST_K and TEST_PACKET.
//!
//! USB test mode is not supported on this device.
//!
//! @return AM_HAL_STATUS_INVALID_OPERATION
//
//*****************************************************************************
extern uint32_t am_hal_usb_test_mode(const void *pHandle, const am_hal_usb_test_mode_e eTestMode);
#endif

//*****************************************************************************
//
//! @brief initialize the endpoint
//!
//! @param pHandle   - handle for the module instance.
//! @param ui8EpAddr - endpoint address including the endpoint direction
//! @param ui8EpAttr - the type of endpoint (bulk/intr/iso/control)
//! @param ui16MaxPacket - the max packet length of the endpoint
//!
//! This function is used by upper layer USB stack to initialize the endpoints
//! when USB host sets the configuration of the USB device.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_ep_init(void *pHandle, uint8_t ui8EpAddr, uint8_t ui8EpAttr, uint16_t ui16MaxPacket);

//*****************************************************************************
//
//! @brief stall the endpoint
//!
//! @param pHandle   - handle for the module instance.
//! @param ui8EpAddr - endpoint address including the endpoint direction
//!
//! This function is used by upper layer USB stack to stall the endpoint when
//! some unknown requests can't be handled or transfer can't continue.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_ep_stall(void *pHandle, uint8_t ui8EpAddr);

//*****************************************************************************
//
//! @brief clear the endpoint stall
//!
//! @param pHandle   - handle for the module instance.
//! @param ui8EpAddr - endpoint address including the endpoint direction
//!
//! This function is used by upper layer USB stack to clear the endpoint stall
//! when USB device stack can recovry from some error status or USB host sends
//! the clear feature request to clear the stall of the endpoint.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_ep_clear_stall(void *pHandle, uint8_t ui8EpAddr);

//*****************************************************************************
//
//! @brief Abort the endpoint transfer
//!
//! @param pHandle Pointer to the USB handle
//! @param ui8EpAddr Endpoint address
//!
//! This function is used by the upper layer USB stack to abort the endpoint
//! transfer when the endpoint is closed.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_ep_xfer_abort(void *pHandle, uint8_t ui8EpAddr);

//*****************************************************************************
//
//! @brief submit a USB transfer
//!
//! @param pHandle   - handle for the module instance.
//! @param ui8EpAddr - endpoint address including the endpoint direction
//! @param pui8Buf   - the buffer for receiving the data from USB host or
//!                    sending data to the USB host. Must be non-NULL when
//!                    \e ui32Len is non-zero; may be NULL for a zero-length
//!                    transfer (ZLP).
//! @param ui32Len   - the length of the buffer.
//!
//! This function is used by the upper layer USB stack to submit a transfer requst
//! to the USB device controller.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_ep_xfer(void *pHandle, uint8_t ui8EpAddr, uint8_t *pui8Buf, uint32_t ui32Len);

//*****************************************************************************
//
//! @brief get the current USB speed
//!
//! @param pHandle - handle for the module instance.
//!
//! This function is used by the upper layer USB stack to get the current USB
//! speed type
//! like full-speed or high-speed.
//!
//! @return one of am_hal_usb_dev_speed_e like
//!    AM_HAL_USB_SPEED_FULL
//!    AM_HAL_USB_SPEED_HIGH, etc.
//
//*****************************************************************************
extern am_hal_usb_dev_speed_e am_hal_get_usb_dev_speed(void *pHandle);

//*****************************************************************************
//
//! @brief set the USB speed
//!
//! @param pHandle - handle for the module instance.
//! @param eSpeed  - speed type of the USB device
//!
//! This function is used by the upper layer USB stack to force USB device controller
//! to run under a certain speed, for example full-speed by ignoring the controller
//! high-speed capability.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_dev_speed(void *pHandle, am_hal_usb_dev_speed_e eSpeed);

//*****************************************************************************
//
//! @brief set the USB PHY clock source and divider
//!
//! @param pHandle - handle for the module instance.
//! @param eClkSrc - clock selection for USB PHY clock source
//! @param eClkDiv - divider ratio for USB PHY clock from the clock source
//!
//! This function should be called before am_hal_usb_phy_clock_enable() to take
//! effect.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_phy_clk_source(void *pHandle, am_hal_usb_phyclksrc_e eClkSrc, am_hal_usb_phyclksrc_div_e eClkDiv);

//*****************************************************************************
//
//! @brief initialize the USB device controller module
//!
//! @param ui32Module - the index to the USB module
//! @param ppHandle   - the handle of initialized USB instance
//!
//! This function should be called first before we use any other USB HAL driver
//! functions, except for am_hal_usb_phy_electrical_tuning_set()
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_initialize(uint32_t ui32Module, void **ppHandle);

//*****************************************************************************
//
//! @brief uninitialize the USB device controller module
//!
//! @param pHandle - the handle of initialized USB instance
//!
//! This function should be called finally after we want to shutdown the USB
//! device controller.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_deinitialize(void *pHandle);

//*****************************************************************************
//
//! @brief power on/off the USB device controller
//!
//! @param pHandle      - the handle of initialized USB instance
//! @param ePowerState  - the power state of USB device controller
//! @param bRetainState - whether retain the USB registers or not
//!
//! This function should be called after USB device controller has been initalized
//! successfully by 'am_hal_usb_initialize'.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_power_control(void *pHandle, am_hal_sysctrl_power_state_e ePowerState, bool bRetainState);

//*****************************************************************************
//
//! @brief Power up the USB PHY
//!
//! Release the USB PHY from power-down, enable the PHY PLL, configure clock
//! selection, and enable UTMI TX output.
//
//*****************************************************************************
extern void am_hal_usb_power_up_usb_phy(void);

//*****************************************************************************
//
//! @brief turn on/off USB PHY reference clock
//!
//! @param pHandle - the handle of initialized USB instance
//! @param eEnable - whether to enable the USB PHY reference clock
//! @param eSpeed  - USB maximum device speed
//!
//! This function is used to configure USB PHY reference clock for the USB
//! device speed mode selected by eSpeed. @n
//! Note: If AM_HAL_USB_SPEED_HIGH is selected for eSpeed, make sure that the
//!       system PLL is configured as USB PHY clock source, and locked before
//!       this function is called.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_phy_clock_enable(void *pHandle, bool bEnable, am_hal_usb_dev_speed_e eSpeed);

//*****************************************************************************
//
//! @brief enable the USB bus's interrupts
//!
//! @param pHandle    - the handle of initialized USB instance
//! @param ui32IntMask - the bit mask of enabled USB bus interrupts
//!
//! This function is used to enable the USB bus interrupts.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_intr_usb_enable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief disable the USB bus's interrupts
//!
//! @param pHandle    - the handle of initialized USB instance
//! @param ui32IntMask - the bit mask of USB bus interrupts.
//!
//! This function is used to disable the USB bus interrupts.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_intr_usb_disable(void *pHandle, uint32_t ui32IntMask);

//*****************************************************************************
//
//! @brief clear the USB bus interrupts
//!
//! @param pHandle - the handle of initialized USB instance
//!
//! This function is used to clear the USB bus interrupt status.
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_intr_usb_clear(void *pHandle);

//*****************************************************************************
//
//! @brief USB bus interrupt masks (`USBIRQ` / `USBIEN`).
//!
//! Values match Atomiq USBIRQ/USBIEN bit positions (CMSIS masks). This is an
//! atomiq110 ABI; Apollo4/5/510 keep their legacy soft packing.
//
//*****************************************************************************
typedef enum
{
    USB_INTRUSB_SUDAVIR_Msk = USB_USBIRQ_USBIRQSUDAVIRQ_Msk, /* bit 0  */
    USB_INTRUSB_SOF_Msk     = USB_USBIRQ_USBIRQSOFIRQ_Msk,    /* bit 1  */
    USB_INTRUSB_Suspend_Msk = USB_USBIRQ_USBIRQSUSPIRQ_Msk,   /* bit 3  */
    USB_INTRUSB_Reset_Msk   = USB_USBIRQ_USBIRQRESIRQ_Msk,    /* bit 4  */
    // #### INTERNAL BEGIN ####
#if AM_HAL_USB_LPM_L1_SUPPORT
    USB_INTRUSB_L1_Msk      = USB_USBIRQ_USBIRQLPMIRQ_Msk,    /* bit 7  */
#endif
    // #### INTERNAL END ####
    USB_INTRUSB_Resume_Msk  = USB_USBIRQ_USBIRQWUIREQ_Msk,    /* bit 15 */
}
am_hal_usb_usbirq_usbext_msk_e;

//*****************************************************************************
//
//! @brief USB IN endpoint interrupt masks (`IN07IRQ` / `IN07IEN` bits).
//!
//! Values match Atomiq IN07IRQOUT07IRQ / IN07IENOUT07IEN (bit N = EPn).
//
//*****************************************************************************
typedef enum
{
    USB_INTRIN_EP0_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN0IRQ_Msk, /* bit 0 */
    USB_INTRIN_EP1_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN1IRQ_Msk, /* bit 1 */
    USB_INTRIN_EP2_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN2IRQ_Msk, /* bit 2 */
    USB_INTRIN_EP3_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN3IRQ_Msk, /* bit 3 */
    USB_INTRIN_EP4_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN4IRQ_Msk, /* bit 4 */
    USB_INTRIN_EP5_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN5IRQ_Msk, /* bit 5 */
    USB_INTRIN_EP6_Msk = USB_IN07IRQOUT07IRQ_IN07IRQIN6IRQ_Msk, /* bit 6 */
}
am_hal_usb_inxirq_msk_e;

//*****************************************************************************
//
//! @brief USB OUT endpoint interrupt masks (`OUT07IRQ` / `OUT07IEN` bits).
//!
//! Values match Atomiq IN07IRQOUT07IRQ / IN07IENOUT07IEN (bit 16+N = EPn).
//
//*****************************************************************************
typedef enum
{
    USB_INTROUT_EP0_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT0IRQ_Msk, /* bit 16 */
    USB_INTROUT_EP1_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT1IRQ_Msk, /* bit 17 */
    USB_INTROUT_EP2_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT2IRQ_Msk, /* bit 18 */
    USB_INTROUT_EP3_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT3IRQ_Msk, /* bit 19 */
    USB_INTROUT_EP4_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT4IRQ_Msk, /* bit 20 */
    USB_INTROUT_EP5_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT5IRQ_Msk, /* bit 21 */
    USB_INTROUT_EP6_Msk = USB_IN07IRQOUT07IRQ_OUT07IRQOUT6IRQ_Msk, /* bit 22 */
}
am_hal_usb_outxirq_msk_e;

//*****************************************************************************
//
//! @brief get all USB related interrupt status
//!
//! This function is used to get all USB related interrupt status.
//!
//! @param pHandle             - the handle of initialized USB instance
//! @param ui32IntrUsbStatus   - USB bus interrupt status (USBIRQ/USBIEN soft = HW)
//! @param ui32IntrEpStatus    - EP IRQ status (IN07IRQOUT07IRQ raw packing)
//! @param ui32IntrAutoDMAStatus - USB Auto DMA interrupt status
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_intr_status_get(void *pHandle, uint32_t *ui32IntrUsbStatus, uint32_t *ui32IntrEpStatus,
                                          uint32_t *ui32IntrAutoDMAStatus);

//*****************************************************************************
//
//! @brief USB interrupt service routine
//!
//! This function is USB interrupt service routine function.
//!
//! @param pHandle             - the handle of initialized USB instance
//! @param ui32IntrUsbStatus   - USB bus interrupt status
//! @param ui32IntrEpStatus    - EP IRQ status (IN at [6:0], OUT at [22:16])
//! @param ui32IntrAutoDMAStatus - USB Auto DMA interrupt status
//!
//
//*****************************************************************************
extern void am_hal_usb_interrupt_service(void *pHandle, uint32_t ui32IntrUsbStatus, uint32_t ui32IntrEpStatus,
                                        uint32_t ui32IntrAutoDMAStatus);

//*****************************************************************************
//
//! @brief Macro to read and handle USB interrupts
//!
//! Application / USB stack can either call this macro, or implement its custom
//! handling when usb isr is received
//
//*****************************************************************************
#define am_hal_usb_handle_isr(pHandle)                  \
    {                                                   \
        uint32_t int_status[3];                         \
        am_hal_usb_intr_status_get(pHandle,             \
                                    &int_status[0],     \
                                    &int_status[1],     \
                                    &int_status[2]);    \
        am_hal_usb_interrupt_service(pHandle,           \
                                     int_status[0],     \
                                     int_status[1],     \
                                     int_status[2]);    \
    }

//*****************************************************************************
//
//! @brief Set USB Xfer Mode
//!
//! @param pHandle - the handle of initialized USB instance
//! @param eXferMode Xfer Mode enum
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_xfer_mode(void *pHandle, am_hal_usb_xfer_mode_e eXferMode);

//*****************************************************************************
//
//! @brief Set the packet-buffer count for a non-control endpoint direction.
//!
//! This function must be called before am_hal_usb_ep_init() for the selected
//! endpoint direction.
//!
//! @param pHandle - the handle of initialized USB instance
//! @param epnum   - EP number for the setting (1 to 6)
//! @param dir     - Direction of the endpoint FIFO for the setting
//! @param eCount  - Endpoint packet-buffer mode (single to quad)
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_set_ep_buffer_count(void *pHandle, uint8_t epnum,
                                               am_hal_usb_xfer_dir_e dir,
                                               am_hal_usb_ep_buffer_count_e eCount);

//*****************************************************************************
//
//! @brief Get the configured packet-buffer count for a non-control endpoint
//!        direction.
//!
//! @param pHandle - the handle of initialized USB instance
//! @param epnum   - EP number for the setting (1 to 6)
//! @param dir     - Direction of the endpoint FIFO for the setting
//! @param peCount - Pointer to receive the configured packet-buffer mode
//!
//! @return one of am_hal_status_e like AM_HAL_STATUS_SUCCESS
//
//*****************************************************************************
extern uint32_t am_hal_usb_get_ep_buffer_count(void *pHandle, uint8_t epnum,
                                               am_hal_usb_xfer_dir_e dir,
                                               am_hal_usb_ep_buffer_count_e *peCount);

//*****************************************************************************
//
#ifdef __cplusplus
}
#endif

#endif /* AM_HAL_USB_H_ */

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
