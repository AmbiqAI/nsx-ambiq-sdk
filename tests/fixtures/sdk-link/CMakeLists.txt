cmake_minimum_required(VERSION 3.20)
project(nsx_sdk_link C CXX ASM)
include("${NSX_TEST_SDK_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(sdk_link
    "${NSX_TEST_SDK_ROOT}/tests/hardware/firmware/main.cpp"
    archive_probe.c)
# Link-only probe: keep real vendor implementations without running peripherals.
target_link_options(sdk_link PRIVATE "LINKER:-u,nsx_sdk_archive_probe")
