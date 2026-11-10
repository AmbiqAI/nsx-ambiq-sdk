cmake_minimum_required(VERSION 3.20)
project(nsx_psram_throughput C CXX ASM)
include("${NSX_TEST_HARNESS_ROOT}/tests/hardware/common/firmware.cmake")
nsx_add_test_firmware(psram_throughput main.c MODULES interrupt psram)
set(NSX_TEST_PSRAM_CLOCK 48000000 CACHE STRING "Diagnostic PSRAM clock in Hz")
option(NSX_TEST_CPU_HIGH "Use the board's supported high CPU mode" OFF)
if(NOT NSX_TEST_PSRAM_CLOCK MATCHES "^(48000000|96000000)$")
    message(FATAL_ERROR "Diagnostic supports PSRAM 48 or 96 MHz only")
endif()
target_compile_definitions(psram_throughput PRIVATE
    NSX_TEST_PSRAM_CLOCK=${NSX_TEST_PSRAM_CLOCK}
    NSX_TEST_CPU_HIGH=$<BOOL:${NSX_TEST_CPU_HIGH}>)
target_link_options(psram_throughput PRIVATE
    "LINKER:--wrap=am_hal_mspi_nonblocking_transfer,--wrap=am_hal_mspi_interrupt_service")
