if(TARGET nsx_sdk_ambiqsuite_r4)
    return()
endif()

add_library(nsx_sdk_ambiqsuite_r4 INTERFACE)
set_target_properties(nsx_sdk_ambiqsuite_r4 PROPERTIES EXPORT_NAME sdk_ambiqsuite_r4)
add_library(nsx::sdk_ambiqsuite_r4 ALIAS nsx_sdk_ambiqsuite_r4)
add_library(nsx::ambiqsuite_r4 ALIAS nsx_sdk_ambiqsuite_r4)

target_compile_definitions(nsx_sdk_ambiqsuite_r4 INTERFACE
    NSX_SDK_AMBIQSUITE_R4
)

install(TARGETS
    nsx_sdk_ambiqsuite_r4
    EXPORT nsxTargets
)
