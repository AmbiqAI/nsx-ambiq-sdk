if(NOT DEFINED NSX_SDK_PROVIDER)
    message(FATAL_ERROR "NSX_SDK_PROVIDER must be defined before nsx-usb is added.")
endif()

if(NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r4")
    if(NOT TARGET nsx_ambiq_usb_r4)
        message(FATAL_ERROR "nsx-usb requires nsx-ambiq-usb-r4 to be added first when NSX_SDK_PROVIDER=ambiqsuite-r4.")
    endif()
    set(NSX_USB_SUBSTRATE_TARGET nsx_ambiq_usb_r4)
    set(NSX_USB_SUBSTRATE_PUBLIC_TARGET nsx::ambiq_usb_r4)
elseif(NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r5")
    if(NOT TARGET nsx_ambiq_usb_r5)
        message(FATAL_ERROR "nsx-usb requires nsx-ambiq-usb-r5 to be added first when NSX_SDK_PROVIDER=ambiqsuite-r5.")
    endif()
    set(NSX_USB_SUBSTRATE_TARGET nsx_ambiq_usb_r5)
    set(NSX_USB_SUBSTRATE_PUBLIC_TARGET nsx::ambiq_usb_r5)
else()
    message(FATAL_ERROR "nsx-usb does not support NSX_SDK_PROVIDER='${NSX_SDK_PROVIDER}'.")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_core)
    set(NSX_USB_CORE_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_core}")
else()
    set(NSX_USB_CORE_DIR "${NSX_ROOT}/modules/nsx-core")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_timer)
    set(NSX_USB_TIMER_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_timer}")
else()
    set(NSX_USB_TIMER_DIR "${NSX_ROOT}/modules/nsx-timer")
endif()

# Descriptor and callback overrides are compiled into the TinyUSB substrate
# archive so the linker sees the strong definitions beside TinyUSB's weak
# callback references.
target_sources(${NSX_USB_SUBSTRATE_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/overrides/nsx_usb_overrides.c
    ${CMAKE_CURRENT_LIST_DIR}/src/overrides/nsx_usb_descriptors.c
)
target_include_directories(${NSX_USB_SUBSTRATE_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
)

add_library(nsx_usb STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_usb.c
)
set_target_properties(nsx_usb PROPERTIES EXPORT_NAME usb)
add_library(nsx::usb ALIAS nsx_usb)

target_link_libraries(nsx_usb PUBLIC
    nsx_soc_hal
    nsx_core
    nsx_timer
    ${NSX_USB_SUBSTRATE_PUBLIC_TARGET}
)

target_include_directories(nsx_usb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_USB_CORE_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_USB_TIMER_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-usb/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-timer/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-usb
)

install(TARGETS
    nsx_usb
    EXPORT nsxTargets
)
