cmake_minimum_required(VERSION 3.20)
project(nsx_cpp_startup C CXX)

include(GNUInstallDirs)
set(NSX_ROOT "${NSX_TEST_SDK_ROOT}")
set(NSX_CMAKE_DIR "${NSX_ROOT}/cmake")
set(NSX_SDK_PROVIDER ambiqsuite)
set(NSX_AMBIQSUITE_VERSION stable-2026.06.18)
set(NSX_AMBIQSUITE_ROOT "${NSX_ROOT}/modules/nsx-ambiqsuite/sdk")
include("${NSX_ROOT}/boards/${NSX_TEST_BOARD}/board.cmake")
add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-core" cmsis-core)
add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-startup" cmsis-startup)

add_executable(constructors main.cpp)
set_target_properties(constructors PROPERTIES SUFFIX .elf CXX_STANDARD 17)
target_compile_options(constructors PRIVATE -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
target_link_libraries(constructors PRIVATE nsx::startup)
target_link_options(constructors PRIVATE
    -nostartfiles
    "LINKER:-T,${NSX_LINKER_SCRIPT}"
    "LINKER:--gc-sections"
)
