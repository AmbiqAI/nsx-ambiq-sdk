if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before nsx-audio is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-audio is added.")
endif()

if(NSX_SOC_FAMILY STREQUAL "apollo5b" OR NSX_SOC_FAMILY STREQUAL "apollo510" OR NSX_SOC_FAMILY STREQUAL "apollo510b" OR NSX_SOC_FAMILY STREQUAL "apollo510L" OR NSX_SOC_FAMILY STREQUAL "apollo330P")
    set(NSX_AUDIO_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/nsx_pdm_apollo5.c)
else()
    message(FATAL_ERROR "Unsupported NSX_SOC_FAMILY for nsx-audio: ${NSX_SOC_FAMILY}. Supported: apollo5b/apollo510/apollo510b/apollo510L/apollo330P.")
endif()

add_library(nsx_audio STATIC ${NSX_AUDIO_SOURCE})
set_target_properties(nsx_audio PROPERTIES EXPORT_NAME audio)
add_library(nsx::audio ALIAS nsx_audio)

target_link_libraries(nsx_audio PUBLIC
    nsx_soc_hal
    nsx_core
)

target_include_directories(nsx_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_ROOT}/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-audio/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-audio
)
install(TARGETS nsx_audio EXPORT nsxTargets)
