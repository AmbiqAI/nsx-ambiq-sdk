cmake_minimum_required(VERSION 3.20)
project(nsx_timer_period C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(timer_period main.c MODULES interrupt timer)
