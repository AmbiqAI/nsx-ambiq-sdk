cmake_minimum_required(VERSION 3.20)
project(nsx_power_link C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(power_link
    "${NSX_TEST_HARNESS_ROOT}/tests/hardware/firmware/main.cpp"
    MODULES interrupt timer power)
# Retain the public entry points without executing power or GPIO operations.
target_link_options(power_link PRIVATE
    "LINKER:-u,nsx_power_monitor_init"
    "LINKER:-u,nsx_power_monitor_set_state"
    "LINKER:-u,nsx_power_configure"
    "LINKER:-u,nsx_power_set_performance_mode"
    "LINKER:-u,nsx_power_profile_dump")
