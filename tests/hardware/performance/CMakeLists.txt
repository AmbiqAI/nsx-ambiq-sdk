cmake_minimum_required(VERSION 3.20)
project(nsx_performance_contract C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(performance_contract main.c MODULES interrupt timer power)
