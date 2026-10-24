# SPDX-FileCopyrightText: Copyright 2026 Ambiq Micro, Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Standalone flashable hardware test for the Atomiq110 STIMER timebase
# behind nsx-ethos-u-driver's bounded Ethos-U85 inference wait
# (modules/nsx-npu/src/atomiq110/nsx_npu_timebase.c).
#
# Unlike tools/nsx_link_smoke.py's link-only smoke (which never links a
# linker script and only proves the module compile/link graph resolves),
# this project produces a genuinely flashable .bin: it wires in
# nsx::cmsis_startup, nsx::core, nsx::npu, and the Atomiq110 linker script,
# then runs the actual timebase timing checks plus a real Ethos-U85 inference
# on hardware. See main.c for the checks and README.md for build/flash
# instructions.
#
# This is a standalone CMake project (its own project()), not something a
# parent add_subdirectory()'s in -- nsx-ambiq-sdk otherwise ships no
# add_executable() anywhere (see cmake/README.md); this is the first.
#
# Usage:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<gcc-toolchain-file>
#   cmake --build build
#   arm-none-eabi-objcopy -O binary build/npu_timebase_hw_test.elf \
#       build/npu_timebase_hw_test.bin
#
# A ready-made gcc toolchain file can be generated with:
#   python3 ../../../../tools/nsx_toolchain_file.py --toolchain-family gcc \
#       --output /tmp/gcc.cmake --gcc-root /usr
cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

project(nsx_npu_timebase_hw_test C ASM)

include(GNUInstallDirs)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

get_filename_component(NSX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(NSX_CMAKE_DIR "${NSX_ROOT}/cmake")
set(NSX_SDK_PROVIDER "ambiqsuite")
set(NSX_TOOLCHAIN_FAMILY "gcc" CACHE STRING "NSX toolchain family (must match -DCMAKE_TOOLCHAIN_FILE)")
set(NSX_AMBIQSUITE_VERSION "stable-2026.06.18")
set(NSX_AMBIQSUITE_ROOT "${NSX_ROOT}/modules/nsx-ambiqsuite/sdk")

# nsx-ethos-u-driver is an external module (materialized by the neuralspotx
# registry in a real downstream project); for this standalone in-tree test,
# point at a local sibling checkout. Override with
# -DNSX_ETHOSU_DRIVER_ROOT=<path> if yours lives elsewhere.
set(NSX_ETHOSU_DRIVER_ROOT "${NSX_ROOT}/../nsx-ethos-u-driver" CACHE PATH
    "Path to a nsx-ethos-u-driver checkout")
if(NOT EXISTS "${NSX_ETHOSU_DRIVER_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "nsx-ethos-u-driver not found at ${NSX_ETHOSU_DRIVER_ROOT}. "
        "Clone it and/or pass -DNSX_ETHOSU_DRIVER_ROOT=<path>.")
endif()

include("${NSX_ROOT}/boards/atomiq110_fpga_turbo/board.cmake")

# Module assembly, in dependency order (mirrors tools/nsx_link_smoke.py plus
# the additional modules a real flashable app needs: startup/linker,
# nsx-core, the Ethos-U driver, and nsx-npu itself).
add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-core" nsx-cmsis-core)
add_subdirectory("${NSX_ROOT}/modules/nsx-ambiq-hal" nsx-ambiq-hal)
add_subdirectory("${NSX_ROOT}/modules/nsx-ambiq-bsp" nsx-ambiq-bsp)
add_subdirectory("${NSX_ROOT}/modules/nsx-soc-hal" nsx-soc-hal)
add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-startup" nsx-cmsis-startup)
add_subdirectory("${NSX_ROOT}/modules/nsx-core" nsx-core)
add_subdirectory("${NSX_ETHOSU_DRIVER_ROOT}" nsx-ethos-u-driver)
add_subdirectory("${NSX_ROOT}/modules/nsx-npu" nsx-npu)

add_executable(npu_timebase_hw_test
    main.c
    model/resnet_v1_8_32_tfs_int8_buffers.c
)
target_include_directories(npu_timebase_hw_test PRIVATE model)

target_link_libraries(npu_timebase_hw_test PRIVATE
    nsx::board
    nsx::cmsis_startup
    nsx::ambiq_bsp
    nsx::soc_hal
    nsx::core
    nsx::npu
)

# Real flashable link: entry point + linker script + map file + gc-sections,
# matching the flags the ambiqsuite build system generates for atomiq110
# examples (see gcc/Makefile's LFLAGS in any generated example project).
target_link_options(npu_timebase_hw_test PRIVATE
    -nostartfiles
    -static
    "-Wl,--gc-sections,--entry,Reset_Handler,-Map,$<TARGET_FILE_DIR:npu_timebase_hw_test>/npu_timebase_hw_test.map"
    -T${NSX_LINKER_SCRIPT}
    -Wl,--start-group -lm -lc -lgcc -lstdc++ -Wl,--end-group
)
set_target_properties(npu_timebase_hw_test PROPERTIES
    LINK_DEPENDS "${NSX_LINKER_SCRIPT}"
)

add_custom_command(TARGET npu_timebase_hw_test POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:npu_timebase_hw_test>
        $<TARGET_FILE_DIR:npu_timebase_hw_test>/npu_timebase_hw_test.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:npu_timebase_hw_test>
    COMMENT "Generating npu_timebase_hw_test.bin"
)

# JLink flash script + "flash" target: `cmake --build build --target flash`
# builds (if needed) then flashes over SWD via JLinkExe, using the standard
# device/if/speed/loadbin/g sequence for this board.
set(NSX_JLINK_USB "1160000024" CACHE STRING "J-Link probe USB serial number")

file(GENERATE
    OUTPUT "$<TARGET_FILE_DIR:npu_timebase_hw_test>/flash_cmds.jlink"
    CONTENT
"device Atomiq110
if swd
speed 1000
connect
r
loadbin $<TARGET_FILE_DIR:npu_timebase_hw_test>/npu_timebase_hw_test.bin 0x22000000
g
"
)

add_custom_target(flash
    COMMAND JLinkExe -USB ${NSX_JLINK_USB}
        -CommandFile $<TARGET_FILE_DIR:npu_timebase_hw_test>/flash_cmds.jlink
    DEPENDS npu_timebase_hw_test
    COMMENT "Flashing npu_timebase_hw_test.bin via JLink (USB ${NSX_JLINK_USB})"
    VERBATIM
)
