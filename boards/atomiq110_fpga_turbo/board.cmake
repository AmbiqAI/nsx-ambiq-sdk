include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/socs/atomiq110.cmake")

set(NSX_AMBIQ_BOARD_NAME "atomiq110_fpga_turbo")
set(NSX_AMBIQ_BSP_LIB_SUBDIR "atomiq110_fpga_turbo")
set(NSX_AMBIQ_BSP_DIR "${NSX_AMBIQSUITE_ROOT}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp")

set(NSX_BOARD_TARGET nsx_board_atomiq110_fpga_turbo)
set(NSX_BOARD_FLAGS_TARGET nsx_board_atomiq110_fpga_turbo_flags)
set(NSX_BOARD_TARGET_EXPORT_NAME "board_atomiq110_fpga_turbo")
set(NSX_BOARD_FLAGS_TARGET_EXPORT_NAME "board_flags_atomiq110_fpga_turbo")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

add_library(${NSX_BOARD_TARGET} INTERFACE)
add_library(${NSX_BOARD_FLAGS_TARGET} INTERFACE)
set_target_properties(${NSX_BOARD_TARGET} PROPERTIES EXPORT_NAME ${NSX_BOARD_TARGET_EXPORT_NAME})
set_target_properties(${NSX_BOARD_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_BOARD_FLAGS_TARGET_EXPORT_NAME})

add_library(nsx::board ALIAS ${NSX_BOARD_TARGET})
add_library(nsx::board_atomiq110_fpga_turbo ALIAS ${NSX_BOARD_TARGET})
add_library(nsx::board_flags ALIAS ${NSX_BOARD_FLAGS_TARGET})

# atomiq110 ships only as the FPGA "turbo" board today. NSX_BOARD_IS_FPGA marks
# the realization so consumers can branch on FPGA-only realities (no HBLRAM,
# reduced peripheral set, DIV-scaled clocks). The HAL/BSP archives are already
# compiled in FPGA mode (ATOMIQ11X_FPGA is baked into am_mcu_apollo.h), so no
# additional define is injected here. The board has no user buttons
# (AM_BSP_NUM_BUTTONS == 0), so it is intentionally given no button facts.
target_compile_definitions(${NSX_BOARD_FLAGS_TARGET} INTERFACE
    atomiq110_fpga_turbo
    NSX_BOARD_IS_FPGA=1
    NSX_BOARD_HAS_BUTTONS=0
    STACK_SIZE=4096
)

target_link_libraries(${NSX_BOARD_FLAGS_TARGET} INTERFACE ${NSX_SOC_FLAGS_TARGET})

target_link_libraries(${NSX_BOARD_TARGET} INTERFACE ${NSX_BOARD_FLAGS_TARGET})

install(TARGETS
    ${NSX_BOARD_TARGET}
    ${NSX_BOARD_FLAGS_TARGET}
    EXPORT nsxTargets
)
