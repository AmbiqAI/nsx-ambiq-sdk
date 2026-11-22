# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, Ambiq
function(nsx_select_prebuilt_archive variable default_path role)
    get_property(cache_type CACHE "${variable}" PROPERTY TYPE)
    get_property(cache_help CACHE "${variable}" PROPERTY HELPSTRING)
    set(auto_help "Selected AmbiqSuite ${role} archive")
    if(NOT DEFINED ${variable} OR (cache_type STREQUAL "INTERNAL" AND cache_help STREQUAL auto_help))
        set(${variable} "${default_path}" CACHE INTERNAL "${auto_help}")
    elseif(cache_help STREQUAL "Path to AmbiqSuite ${role} static library")
        # Older wrappers stored both explicit overrides and defaults as FILEPATH.
        if("${${variable}}" STREQUAL "$CACHE{${variable}}" AND NOT "${${variable}}" STREQUAL "${default_path}")
            message(FATAL_ERROR
                "${variable} has a legacy cached path '$CACHE{${variable}}'; the selected "
                "SDK default is '${default_path}'. Reconfigure with -U${variable} to "
                "select the SDK default, or -D${variable}:FILEPATH=<path> to pin an archive.")
        endif()
    elseif(cache_type STREQUAL "UNINITIALIZED")
        set(${variable} "$CACHE{${variable}}" CACHE FILEPATH "Explicit AmbiqSuite ${role} archive")
    endif()
endfunction()
