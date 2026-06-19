if(NOT DEFINED NSX_SDK_PROVIDER)
    message(FATAL_ERROR "NSX_SDK_PROVIDER must be defined before nsx-usb is added.")
endif()

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR "nsx-usb does not support NSX_SDK_PROVIDER='${NSX_SDK_PROVIDER}'.")
endif()

# The USB substrate (TinyUSB DCD) is provided by the unified nsx-ambiq-usb
# module, which selects the SoC-family-specific device-controller port
# internally. It must be added to the app module graph before nsx-usb.
if(TARGET nsx_ambiq_usb)
    set(NSX_USB_SUBSTRATE_TARGET nsx_ambiq_usb)
    set(NSX_USB_SUBSTRATE_PUBLIC_TARGET nsx::ambiq_usb)
else()
    message(FATAL_ERROR
        "nsx-usb requires the AmbiqSuite USB substrate (nsx-ambiq-usb) to be "
        "added before nsx-usb.")
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
    nsx::soc_hal
    nsx::core
    nsx::timer
    ${NSX_USB_SUBSTRATE_PUBLIC_TARGET}
)

target_include_directories(nsx_usb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-usb/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-usb
)

install(TARGETS
    nsx_usb
    EXPORT nsxTargets
)
