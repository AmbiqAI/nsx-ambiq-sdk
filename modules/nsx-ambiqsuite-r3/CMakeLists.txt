if(TARGET nsx_sdk_ambiqsuite_r3)
    return()
endif()

add_library(nsx_sdk_ambiqsuite_r3 INTERFACE)
set_target_properties(nsx_sdk_ambiqsuite_r3 PROPERTIES EXPORT_NAME sdk_ambiqsuite_r3)
add_library(nsx::sdk_ambiqsuite_r3 ALIAS nsx_sdk_ambiqsuite_r3)
add_library(nsx::ambiqsuite_r3 ALIAS nsx_sdk_ambiqsuite_r3)

target_compile_definitions(nsx_sdk_ambiqsuite_r3 INTERFACE
    NSX_SDK_AMBIQSUITE_R3
)

install(TARGETS
    nsx_sdk_ambiqsuite_r3
    EXPORT nsxTargets
)
