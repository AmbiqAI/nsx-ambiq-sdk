include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/socs/apollo4p.cmake")

set(NSX_AMBIQ_BOARD_NAME "apollo4p_evb")
set(NSX_AMBIQ_BSP_LIB_SUBDIR "evb")
set(NSX_AMBIQ_BSP_DIR "${NSX_AMBIQSUITE_ROOT}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp")

set(NSX_BOARD_TARGET nsx_board_apollo4p_evb)
set(NSX_BOARD_FLAGS_TARGET nsx_board_apollo4p_evb_flags)
set(NSX_BOARD_TARGET_EXPORT_NAME "board_apollo4p_evb")
set(NSX_BOARD_FLAGS_TARGET_EXPORT_NAME "board_flags_apollo4p_evb")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")

add_library(${NSX_BOARD_TARGET} INTERFACE)
add_library(${NSX_BOARD_FLAGS_TARGET} INTERFACE)
set_target_properties(${NSX_BOARD_TARGET} PROPERTIES EXPORT_NAME ${NSX_BOARD_TARGET_EXPORT_NAME})
set_target_properties(${NSX_BOARD_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_BOARD_FLAGS_TARGET_EXPORT_NAME})

add_library(nsx::board ALIAS ${NSX_BOARD_TARGET})
add_library(nsx::board_apollo4p_evb ALIAS ${NSX_BOARD_TARGET})
add_library(nsx::board_flags ALIAS ${NSX_BOARD_FLAGS_TARGET})

target_compile_definitions(${NSX_BOARD_FLAGS_TARGET} INTERFACE
    $<$<COMPILE_LANGUAGE:C>:apollo4p_evb>
    $<$<COMPILE_LANGUAGE:C>:AM_PACKAGE_BGA>
    $<$<COMPILE_LANGUAGE:C>:STACK_SIZE=4096>
    # Board button facts: pins owned by the BSP; edge/IRQ mechanism lives in nsx-gpio.
    $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_HAS_BUTTONS=1>
    $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_BUTTON_COUNT=2>
    $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_BUTTON0_PIN=AM_BSP_GPIO_BUTTON0>
    $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_BUTTON1_PIN=AM_BSP_GPIO_BUTTON1>
)

target_link_libraries(${NSX_BOARD_FLAGS_TARGET} INTERFACE ${NSX_SOC_FLAGS_TARGET})
target_link_libraries(${NSX_BOARD_TARGET} INTERFACE ${NSX_BOARD_FLAGS_TARGET})

install(TARGETS
    ${NSX_BOARD_TARGET}
    ${NSX_BOARD_FLAGS_TARGET}
    EXPORT nsxTargets
)
