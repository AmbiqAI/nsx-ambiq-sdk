function(nsx_add_test_firmware target)
    include(GNUInstallDirs)
    set(NSX_ROOT "${NSX_TEST_SDK_ROOT}")
    set(NSX_CMAKE_DIR "${NSX_ROOT}/cmake")
    set(NSX_SDK_PROVIDER ambiqsuite)
    set(NSX_TOOLCHAIN_FAMILY gcc)
    set(NSX_AMBIQSUITE_ROOT "${NSX_ROOT}/modules/nsx-ambiqsuite/sdk")
    include("${NSX_ROOT}/boards/${NSX_TEST_BOARD}/board.cmake")
    add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-core" cmsis-core)
    if(NSX_TEST_STARTUP_OVERRIDE)
        set(NSX_STARTUP_SOURCE "${NSX_TEST_STARTUP_OVERRIDE}")
    endif()
    add_subdirectory("${NSX_ROOT}/modules/nsx-cmsis-startup" startup)
    add_subdirectory("${NSX_ROOT}/modules/nsx-ambiq-hal" hal)
    add_subdirectory("${NSX_ROOT}/modules/nsx-ambiq-bsp" bsp)
    add_subdirectory("${NSX_ROOT}/modules/nsx-soc-hal" soc-hal)
    add_subdirectory("${NSX_ROOT}/modules/nsx-core" core)
    target_link_libraries(nsx_startup PUBLIC nsx::runtime)
    add_executable(${target} ${ARGN}
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/result_reporter.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/unity/unity.c")
    set_target_properties(${target} PROPERTIES SUFFIX .elf CXX_STANDARD 17)
    target_compile_definitions(${target} PRIVATE UNITY_INCLUDE_CONFIG_H)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../vendor/unity" "${CMAKE_BINARY_DIR}")
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
    target_link_libraries(${target} PRIVATE nsx::board nsx::cmsis_startup nsx::ambiq_bsp nsx::soc_hal)
    target_link_options(${target} PRIVATE -nostartfiles "LINKER:-T,${NSX_LINKER_SCRIPT}"
        "LINKER:-u,Reset_Handler" "LINKER:--gc-sections")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/inputs.txt" CONTENT
    "${NSX_STARTUP_SOURCE}\n${NSX_SYSTEM_SOURCE}\n${NSX_LINKER_SCRIPT}\n${NSX_HAL_LIB}\n${NSX_BSP_LIB}\n")
    target_link_options(${target} PRIVATE
        "LINKER:--wrap=_write_r,--wrap=_read_r,--wrap=_close_r,--wrap=_lseek_r,--wrap=_kill_r,--wrap=_getpid_r,--wrap=_fstat_r,--wrap=_isatty_r")

    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/probe.txt" CONTENT "${NSX_SEGGER_DEVICE}\n${NSX_SEGGER_IF_SPEED}\n")
endfunction()
