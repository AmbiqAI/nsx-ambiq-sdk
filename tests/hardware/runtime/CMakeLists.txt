cmake_minimum_required(VERSION 3.20)
project(nsx_hardware_runtime C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(runtime_test main.c)
# Derive bounds from the selected linker output, never duplicate silicon maps.
target_link_options(runtime_test PRIVATE
    "LINKER:--defsym=__nsx_test_stack_start=ADDR(.stack)"
    "LINKER:--defsym=__nsx_test_stack_end=ADDR(.stack)+SIZEOF(.stack)")
if(NSX_TEST_FAULT STREQUAL "runtime-no-svc")
    target_compile_definitions(runtime_test PRIVATE NSX_TEST_NO_SVC_OBSERVATION)
elseif(NOT NSX_TEST_FAULT STREQUAL "none")
    message(FATAL_ERROR "Unknown runtime fault mode")
endif()
