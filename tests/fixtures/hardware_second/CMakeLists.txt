cmake_minimum_required(VERSION 3.20)
project(nsx_synthetic_suite C CXX ASM)
include("${NSX_TEST_SDK_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(second_suite main.c)
