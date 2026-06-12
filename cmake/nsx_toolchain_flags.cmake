function(nsx_assert_file_exists path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Required NSX file does not exist: ${path}")
    endif()
endfunction()

function(nsx_assert_directory_exists path)
    if(NOT IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Required NSX directory does not exist: ${path}")
    endif()
endfunction()

function(nsx_assert_path_component var_name)
    if(NOT DEFINED ${var_name})
        message(FATAL_ERROR "${var_name} must be defined before constructing NSX SDK paths.")
    endif()
    set(value "${${var_name}}")
    if(value STREQUAL "")
        message(FATAL_ERROR "${var_name} must not be empty when constructing NSX SDK paths.")
    endif()
    if(value MATCHES "[/\\\\]" OR value MATCHES "(^|/)\.\.($|/)")
        message(FATAL_ERROR "${var_name}='${value}' must be a single path component.")
    endif()
endfunction()

function(nsx_require_enum_value var_name)
    if(NOT DEFINED ${var_name})
        message(FATAL_ERROR "${var_name} must be set.")
    endif()
    set(value "${${var_name}}")
    list(FIND ARGN "${value}" value_index)
    if(value_index EQUAL -1)
        string(REPLACE ";" ", " allowed_values "${ARGN}")
        message(FATAL_ERROR "Unsupported ${var_name}='${value}'. Expected one of: ${allowed_values}.")
    endif()
endfunction()

# Select the active linker script from the named NSX_LINKER_PROFILE. Precedence:
#   1. An explicit NSX_LINKER_SCRIPT set by the caller always wins.
#   2. NSX_LINKER_PROFILE picks one of the built-in profiles (default, itcm).
#   3. When unset, the "default" profile is used.
# This indirection lets callers opt into curated layouts (e.g. itcm) by name and
# leaves room for future profiles without changing the SoC files.
function(nsx_select_linker_script)
    cmake_parse_arguments(NSX_LS "" "DEFAULT;ITCM" "" ${ARGN})
    if(NOT DEFINED NSX_LS_DEFAULT)
        message(FATAL_ERROR "nsx_select_linker_script requires a DEFAULT script path.")
    endif()

    if(DEFINED NSX_LINKER_SCRIPT)
        return()
    endif()

    if(NOT DEFINED NSX_LINKER_PROFILE OR NSX_LINKER_PROFILE STREQUAL "")
        set(NSX_LINKER_PROFILE "default")
    endif()
    nsx_require_enum_value(NSX_LINKER_PROFILE ${NSX_LINKER_PROFILES})

    if(NSX_LINKER_PROFILE STREQUAL "itcm")
        if(NOT DEFINED NSX_LS_ITCM)
            message(FATAL_ERROR "NSX_LINKER_PROFILE='itcm' is not supported for this SoC/toolchain.")
        endif()
        set(NSX_LINKER_SCRIPT "${NSX_LS_ITCM}" PARENT_SCOPE)
    else()
        set(NSX_LINKER_SCRIPT "${NSX_LS_DEFAULT}" PARENT_SCOPE)
    endif()
endfunction()

set(NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES gcc atfe armclang)
set(NSX_NEWLIB_TOOLCHAIN_FAMILIES gcc atfe)

# Supported linker-script profiles. "default" keeps the standard SBL layout;
# "itcm" promotes the hot inference kernels into ITCM. The list is the single
# source of truth so new profiles can be added without breaking existing
# callers. Selection is driven by the NSX_LINKER_PROFILE cache/cmd-line value.
set(NSX_LINKER_PROFILES default itcm)
set(NSX_SOC_FAMILIES_APOLLO5 apollo5b apollo510 apollo510b apollo510L)
set(NSX_SOC_FAMILIES_APOLLO330 apollo330P)
set(NSX_SOC_FAMILIES_APOLLO4 apollo4l apollo4p)
set(NSX_SOC_FAMILIES_APOLLO3 apollo3 apollo3p)
set(NSX_SOC_FAMILIES_APOLLO2 apollo2)

function(nsx_toolchain_uses_newlib out_var)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})
    set(result FALSE)
    if(NSX_TOOLCHAIN_FAMILY IN_LIST NSX_NEWLIB_TOOLCHAIN_FAMILIES)
        set(result TRUE)
    endif()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(nsx_toolchain_is_armclang out_var)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})
    set(result FALSE)
    if(NSX_TOOLCHAIN_FAMILY STREQUAL "armclang")
        set(result TRUE)
    endif()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(nsx_select_soc_arch_dir out_var)
    if(NOT DEFINED NSX_SOC_FAMILY)
        message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before selecting an NSX implementation directory.")
    endif()

    cmake_parse_arguments(NSX_SOC_ARCH "" "APOLLO5;APOLLO330;APOLLO4;APOLLO3;APOLLO2" "" ${ARGN})
    set(result "")
    if(DEFINED NSX_SOC_ARCH_APOLLO5 AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO5)
        set(result "${NSX_SOC_ARCH_APOLLO5}")
    elseif(DEFINED NSX_SOC_ARCH_APOLLO330 AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO330)
        set(result "${NSX_SOC_ARCH_APOLLO330}")
    elseif(DEFINED NSX_SOC_ARCH_APOLLO4 AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO4)
        set(result "${NSX_SOC_ARCH_APOLLO4}")
    elseif(DEFINED NSX_SOC_ARCH_APOLLO3 AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO3)
        set(result "${NSX_SOC_ARCH_APOLLO3}")
    elseif(DEFINED NSX_SOC_ARCH_APOLLO2 AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO2)
        set(result "${NSX_SOC_ARCH_APOLLO2}")
    endif()

    if(result STREQUAL "")
        message(FATAL_ERROR "Unsupported NSX_SOC_FAMILY='${NSX_SOC_FAMILY}' for this module.")
    endif()

    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(nsx_validate_prebuilt_abi)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})
    if(DEFINED NSX_FLOAT_ABI AND NOT NSX_FLOAT_ABI STREQUAL "hard")
        message(FATAL_ERROR "AmbiqSuite R5 prebuilt artifacts require hard-float ABI; got NSX_FLOAT_ABI='${NSX_FLOAT_ABI}'.")
    endif()
endfunction()

# Single source of truth for the "ATFE links the GCC prebuilt vendor archives"
# policy, consumed consistently by the R3/R4/R5 prebuilt HAL+BSP modules.
#
# TEMPORARY WORKAROUND (USB CDC regression): the ATFE-rebuilt AmbiqSuite HAL
# archive breaks USB CDC EP0 enumeration on Apollo510 (device enumerates but
# descriptor reads fail with -110). The GCC prebuilt HAL/BSP archives are
# AAPCS-compatible, link cleanly into ATFE (clang/lld) firmware, and enumerate
# correctly. Remove this policy (and the call sites) once the ATFE prebuilt is
# fixed at the AmbiqSuite source level. See nsx-ap510-runtime-validation note.
function(nsx_atfe_prefers_gcc_prebuilt out_var)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})

    if(NSX_TOOLCHAIN_FAMILY STREQUAL "atfe")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(nsx_resolve_ambiqsuite_artifact_toolchain out_var)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})

    set(nsx_artifact_toolchain "${NSX_TOOLCHAIN_FAMILY}")
    if(NSX_TOOLCHAIN_FAMILY STREQUAL "armclang")
        set(nsx_artifact_toolchain "acfe")
    endif()

    # Apply the shared ATFE->GCC prebuilt policy (see nsx_atfe_prefers_gcc_prebuilt).
    # Blast radius: this resolver is consumed only by nsx-ambiq-hal-r5 and
    # nsx-ambiq-bsp-r5; for atfe it maps the artifact directory (lib/<toolchain>/)
    # to the gcc variant. Any future caller inherits the same policy by design.
    nsx_atfe_prefers_gcc_prebuilt(nsx_atfe_use_gcc)
    if(nsx_atfe_use_gcc)
        set(nsx_artifact_toolchain "gcc")
    endif()

    set(${out_var} "${nsx_artifact_toolchain}" PARENT_SCOPE)
endfunction()

# Resolve the static-library suffix for a prebuilt AmbiqSuite archive. The R4
# per-family modules link prebuilt archives by name and need the toolchain's
# native suffix (.a for newlib toolchains, .lib for armclang).
function(nsx_resolve_ambiqsuite_archive_suffix out_var)
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})

    set(nsx_archive_suffix ".a")
    if(NSX_TOOLCHAIN_FAMILY STREQUAL "armclang")
        set(nsx_archive_suffix ".lib")
    endif()

    set(${out_var} "${nsx_archive_suffix}" PARENT_SCOPE)
endfunction()

function(nsx_apply_toolchain_flags target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Cannot apply NSX toolchain flags to missing target '${target}'.")
    endif()
    nsx_require_enum_value(NSX_TOOLCHAIN_FAMILY ${NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES})
    if(NOT DEFINED NSX_CPU)
        message(FATAL_ERROR "NSX_CPU must be set before applying NSX toolchain flags.")
    endif()
    if(NOT DEFINED NSX_FLOAT_ABI)
        set(NSX_FLOAT_ABI "hard")
    endif()

    # Architecture-specific fragments derived from the SoC core. The unified SDK
    # spans Cortex-M4F (Apollo3/Apollo4) and Cortex-M55 (Apollo5/510/330P), so the
    # FPU spelling and clang target triple are selected per core rather than pinned
    # to a single family.
    if(NSX_CPU MATCHES "m55")
        set(nsx_gcc_fpu "fpv5-sp-d16")
        set(nsx_clang_fpu "fp-armv8-fullfp16-sp-d16")
        set(nsx_atfe_target "thumbv8.1m.main-unknown-none-eabihf")
        set(nsx_armclang_asm "")
    else()
        # Cortex-M4F (armv7e-m): Apollo3/Apollo4.
        set(nsx_gcc_fpu "fpv4-sp-d16")
        set(nsx_clang_fpu "fpv4-sp-d16")
        set(nsx_atfe_target "thumbv7m-unknown-none-eabihf")
        set(nsx_armclang_asm --cpu=Cortex-M4 --fpu=FPv4-SP --apcs=/hardfp --thumb)
    endif()

    if(NSX_TOOLCHAIN_FAMILY STREQUAL "gcc")
        set(nsx_compile_flags -mcpu=${NSX_CPU} -mthumb -mfloat-abi=${NSX_FLOAT_ABI} -mfpu=${nsx_gcc_fpu})
        set(nsx_link_flags ${nsx_compile_flags})
    elseif(NSX_TOOLCHAIN_FAMILY STREQUAL "atfe")
        set(nsx_compile_flags --target=${nsx_atfe_target} -mcpu=${NSX_CPU} -mthumb -mfloat-abi=${NSX_FLOAT_ABI} -mfpu=${nsx_clang_fpu})
        set(nsx_link_flags ${nsx_compile_flags})
    elseif(NSX_TOOLCHAIN_FAMILY STREQUAL "armclang")
        set(nsx_compile_flags --target=arm-arm-none-eabi -mcpu=${NSX_CPU} -mthumb -mfloat-abi=${NSX_FLOAT_ABI} -mfpu=${nsx_clang_fpu})
        set(nsx_link_flags --cpu=${NSX_CPU})
    else()
        message(FATAL_ERROR "Unsupported NSX_TOOLCHAIN_FAMILY='${NSX_TOOLCHAIN_FAMILY}'.")
    endif()

    target_compile_options(${target} INTERFACE ${nsx_compile_flags})
    if(NSX_TOOLCHAIN_FAMILY STREQUAL "armclang" AND nsx_armclang_asm)
        foreach(nsx_asm_flag IN LISTS nsx_armclang_asm)
            target_compile_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:ASM>:${nsx_asm_flag}>)
        endforeach()
    endif()
    target_link_options(${target} INTERFACE ${nsx_link_flags})
endfunction()
