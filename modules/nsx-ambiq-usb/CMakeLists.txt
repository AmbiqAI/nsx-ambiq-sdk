if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before nsx-ambiq-usb is added.")
endif()
nsx_require_target(nsx::board_flags nsx-ambiq-usb)
nsx_require_target(nsx::ambiq_hal nsx-ambiq-usb)
if(NOT DEFINED NSX_SOC_FAMILY OR NOT DEFINED NSX_AMBIQ_BSP_DIR)
    message(FATAL_ERROR "NSX_SOC_FAMILY and NSX_AMBIQ_BSP_DIR must be defined before nsx-ambiq-usb is added.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR "nsx-ambiq-usb requires NSX_SDK_PROVIDER=ambiqsuite.")
endif()

set(NSX_AMBIQ_USB_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk")
set(NSX_AMBIQ_USB_SRC_DIR "${NSX_AMBIQ_USB_ROOT}/src")
set(NSX_AMBIQ_USB_TINYUSB_SOURCE_DIR "${NSX_AMBIQ_USB_ROOT}/third_party/tinyusb/source")
set(NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR "${NSX_AMBIQ_USB_TINYUSB_SOURCE_DIR}/src")

nsx_assert_directory_exists("${NSX_AMBIQ_USB_SRC_DIR}/usb")
nsx_assert_directory_exists("${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}")

# The TinyUSB device-controller driver (DCD) and CFG_TUSB_MCU value are
# SoC-family specific. Apollo4P uses the Apollo4 port; the Apollo5 generations
# (apollo330P/apollo510/apollo510b/apollo510L/apollo5b) share the Apollo5 port.
if(NSX_SOC_FAMILY STREQUAL "apollo4p")
    set(NSX_AMBIQ_USB_DCD_SOURCE "${NSX_AMBIQ_USB_SRC_DIR}/usb/dcd_apollo4.c")
    set(NSX_AMBIQ_USB_TUSB_MCU 9001)
elseif(NSX_SOC_FAMILY STREQUAL "apollo5b" OR NSX_SOC_FAMILY STREQUAL "apollo510" OR NSX_SOC_FAMILY STREQUAL "apollo510b" OR NSX_SOC_FAMILY STREQUAL "apollo510L" OR NSX_SOC_FAMILY STREQUAL "apollo330P")
    set(NSX_AMBIQ_USB_DCD_SOURCE "${NSX_AMBIQ_USB_SRC_DIR}/usb/dcd_apollo5.c")
    set(NSX_AMBIQ_USB_TUSB_MCU 9002)
else()
    message(FATAL_ERROR "nsx-ambiq-usb does not support NSX_SOC_FAMILY='${NSX_SOC_FAMILY}'.")
endif()

add_library(nsx_ambiq_usb STATIC
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/tusb.c
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/usbd.c
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/usbd_control.c
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/cdc_device_nsx.c
    ${NSX_AMBIQ_USB_DCD_SOURCE}
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/tusb_fifo.c
    ${NSX_AMBIQ_USB_SRC_DIR}/usb/vendor_device.c
)
set_target_properties(nsx_ambiq_usb PROPERTIES EXPORT_NAME ambiq_usb)
add_library(nsx::ambiq_usb ALIAS nsx_ambiq_usb)
add_library(nsx::tinyusb ALIAS nsx_ambiq_usb)

target_link_libraries(nsx_ambiq_usb PUBLIC
    nsx::board_flags
    nsx::ambiq_hal
    nsx::ambiq_bsp
)

target_include_directories(nsx_ambiq_usb PUBLIC
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_SRC_DIR}>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_SRC_DIR}/usb>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}/common>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}/class/cdc>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}/class/vendor>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}/device>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_TINYUSB_INCLUDE_DIR}/osal>
    $<BUILD_INTERFACE:${NSX_AMBIQ_BSP_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/src/usb>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src/common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src/class/cdc>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src/class/vendor>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src/device>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb/third_party/tinyusb/source/src/osal>
)

target_compile_definitions(nsx_ambiq_usb PUBLIC
    NSX_AMBIQ_USB
    CFG_TUSB_MCU=${NSX_AMBIQ_USB_TUSB_MCU}
)

install(DIRECTORY
    ${NSX_AMBIQ_USB_SRC_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb
)
install(DIRECTORY
    ${NSX_AMBIQ_USB_ROOT}/third_party
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb
)
install(TARGETS
    nsx_ambiq_usb
    EXPORT nsxTargets
)
