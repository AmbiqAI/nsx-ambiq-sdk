if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before nsx-ambiq-usb-r4 is added.")
endif()
if(NOT TARGET nsx::board_flags)
    message(FATAL_ERROR "Board must define nsx::board_flags before nsx-ambiq-usb-r4 is added.")
endif()
if(NOT TARGET nsx::ambiq_hal_r4)
    message(FATAL_ERROR "nsx-ambiq-usb-r4 requires nsx-ambiq-hal-r4 to be added first.")
endif()
if(NOT TARGET nsx::ambiq_bsp_r4)
    message(FATAL_ERROR "nsx-ambiq-usb-r4 requires nsx-ambiq-bsp-r4 to be added first.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY OR NOT DEFINED NSX_AMBIQ_BSP_DIR)
    message(FATAL_ERROR "NSX_SOC_FAMILY and NSX_AMBIQ_BSP_DIR must be defined before nsx-ambiq-usb-r4 is added.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r4")
    message(FATAL_ERROR "nsx-ambiq-usb-r4 requires NSX_SDK_PROVIDER=ambiqsuite-r4.")
endif()

set(NSX_AMBIQ_USB_R4_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk")
set(NSX_AMBIQ_USB_R4_SRC_DIR "${NSX_AMBIQ_USB_R4_ROOT}/src")
set(NSX_AMBIQ_USB_R4_TINYUSB_SOURCE_DIR "${NSX_AMBIQ_USB_R4_ROOT}/third_party/tinyusb/source")
set(NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR "${NSX_AMBIQ_USB_R4_TINYUSB_SOURCE_DIR}/src")

nsx_assert_directory_exists("${NSX_AMBIQ_USB_R4_SRC_DIR}/usb")
nsx_assert_directory_exists("${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}")

add_library(nsx_ambiq_usb_r4 STATIC
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/tusb.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/usbd.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/usbd_control.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/cdc_device_nsx.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/dcd_apollo4.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/tusb_fifo.c
    ${NSX_AMBIQ_USB_R4_SRC_DIR}/usb/vendor_device.c
)
set_target_properties(nsx_ambiq_usb_r4 PROPERTIES EXPORT_NAME ambiq_usb_r4)
add_library(nsx::ambiq_usb_r4 ALIAS nsx_ambiq_usb_r4)
add_library(nsx::tinyusb ALIAS nsx_ambiq_usb_r4)

target_link_libraries(nsx_ambiq_usb_r4 PUBLIC
    nsx::board_flags
    nsx::ambiq_hal_r4
    nsx::ambiq_bsp_r4
)

target_include_directories(nsx_ambiq_usb_r4 PUBLIC
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_SRC_DIR}>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_SRC_DIR}/usb>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}/common>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}/class/cdc>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}/class/vendor>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}/device>
    $<BUILD_INTERFACE:${NSX_AMBIQ_USB_R4_TINYUSB_INCLUDE_DIR}/osal>
    $<BUILD_INTERFACE:${NSX_AMBIQ_BSP_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/src/usb>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src/common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src/class/cdc>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src/class/vendor>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src/device>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4/third_party/tinyusb/source/src/osal>
)

target_compile_definitions(nsx_ambiq_usb_r4 PUBLIC NSX_AMBIQ_USB_R4)

if(NSX_SOC_FAMILY STREQUAL "apollo4p")
    target_compile_definitions(nsx_ambiq_usb_r4 PUBLIC CFG_TUSB_MCU=9001)
else()
    message(FATAL_ERROR "nsx-ambiq-usb-r4 does not support NSX_SOC_FAMILY='${NSX_SOC_FAMILY}'.")
endif()

install(DIRECTORY
    ${NSX_AMBIQ_USB_R4_SRC_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4
)
install(DIRECTORY
    ${NSX_AMBIQ_USB_R4_ROOT}/third_party
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiq-usb-r4
)
install(TARGETS
    nsx_ambiq_usb_r4
    EXPORT nsxTargets
)
