if(TARGET nsx_sdk_ambiqsuite_r2)
    return()
endif()

add_library(nsx_sdk_ambiqsuite_r2 INTERFACE)
set_target_properties(nsx_sdk_ambiqsuite_r2 PROPERTIES EXPORT_NAME sdk_ambiqsuite_r2)
add_library(nsx::sdk_ambiqsuite_r2 ALIAS nsx_sdk_ambiqsuite_r2)
add_library(nsx::ambiqsuite_r2 ALIAS nsx_sdk_ambiqsuite_r2)

target_compile_definitions(nsx_sdk_ambiqsuite_r2 INTERFACE
    NSX_SDK_AMBIQSUITE_R2
)

install(TARGETS
    nsx_sdk_ambiqsuite_r2
    EXPORT nsxTargets
)
