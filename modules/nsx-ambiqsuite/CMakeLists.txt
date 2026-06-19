if(TARGET nsx_sdk_ambiqsuite)
    return()
endif()

add_library(nsx_sdk_ambiqsuite INTERFACE)
set_target_properties(nsx_sdk_ambiqsuite PROPERTIES EXPORT_NAME sdk_ambiqsuite)
add_library(nsx::sdk_ambiqsuite ALIAS nsx_sdk_ambiqsuite)
add_library(nsx::ambiqsuite ALIAS nsx_sdk_ambiqsuite)

target_compile_definitions(nsx_sdk_ambiqsuite INTERFACE
    NSX_SDK_AMBIQSUITE
)

install(TARGETS
    nsx_sdk_ambiqsuite
    EXPORT nsxTargets
)
