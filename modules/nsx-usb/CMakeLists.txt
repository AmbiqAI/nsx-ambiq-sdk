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
#
# NOTE: being in the same static archive is NOT sufficient on its own to
# guarantee the strong override wins. GNU ld only extracts an archive member
# when it resolves a currently-outstanding undefined symbol; once ANY
# definition (even a weak one, e.g. usbd.c's own
# `TU_ATTR_WEAK bool tud_vendor_control_xfer_cb(...)` stub, or the
# tud_mount_cb/tud_umount_cb/tud_suspend_cb/tud_resume_cb stubs) satisfies a
# symbol reference during archive scanning, ld does not keep scanning later
# archive members for a stronger definition of the same symbol name. If
# usbd.c.o happens to be extracted from this archive before
# nsx_usb_overrides.c.o (which is common, since usbd.c.o is needed for
# unrelated symbols such as tud_task/tud_init), every weak callback stub it
# defines silently "wins" and the overrides in nsx_usb_overrides.c are never
# linked in -- with no build warning or error. This was confirmed on hardware
# via `nm`: tud_vendor_control_xfer_cb, tud_mount_cb, tud_umount_cb,
# tud_suspend_cb, and tud_resume_cb all resolved as weak (`W`) instead of
# strong (`T`) symbols, meaning connect/disconnect handling and the WebUSB
# control-transfer-based connect path (the one real browsers use via
# `device.controlTransferOut()`, as opposed to a manual bulk "kick" packet)
# never actually ran the app's own logic.
#
# Force every member of the substrate archive to be linked unconditionally
# with --whole-archive/--no-whole-archive so normal strong-beats-weak symbol
# resolution applies deterministically, independent of archive scan order.
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
    -Wl,--whole-archive
    ${NSX_USB_SUBSTRATE_PUBLIC_TARGET}
    -Wl,--no-whole-archive
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
