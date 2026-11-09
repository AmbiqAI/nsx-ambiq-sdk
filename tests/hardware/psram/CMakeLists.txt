cmake_minimum_required(VERSION 3.20)
project(nsx_psram_blocking C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(psram_blocking main.c MODULES interrupt psram)
