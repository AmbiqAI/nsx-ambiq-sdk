cmake_minimum_required(VERSION 3.20)
project(nsx_power_status_smoke C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(power_status_test main.c DEPENDENCIES interrupt timer MODULES power)
