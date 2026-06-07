if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before nsx-core is added.")
endif()

nsx_select_soc_arch_dir(NSX_CORE_ARCH_DIR
    APOLLO5 apollo510
    APOLLO330 apollo510
    APOLLO4 apollo4p
    APOLLO3 apollo3p
    APOLLO2 apollo2
)

# ===================================================================
# nsx-core is composed of cohesive sub-libraries, each owning a single
# concern. The legacy `nsx::core` target is preserved as an umbrella that
# links them all, so existing consumers keep working unchanged.
#
#   nsx::compiler       toolchain-portability macros (header-only)
#   nsx::util           byte ring buffer + safe-memcpy helpers
#   nsx::mem            memory-placement macros + I/D cache control
#   nsx::core_lifecycle runtime init + API versioning (the real "core")
#   nsx::runtime        printf / delay / interrupt / debug shims
#   nsx::system         system bring-up + per-SoC backend + newlib retarget/heap
#   nsx::core           umbrella over all of the above (backward compatible)
# ===================================================================

# Shared flat include interface — every sub-library exposes includes-api so
# headers resolve uniformly for internal and external consumers.
add_library(nsx_core_includes INTERFACE)
set_target_properties(nsx_core_includes PROPERTIES EXPORT_NAME core_includes)
add_library(nsx::core_includes ALIAS nsx_core_includes)
target_include_directories(nsx_core_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
)

# ---- nsx::compiler — toolchain portability (header-only) ----
add_library(nsx_compiler INTERFACE)
set_target_properties(nsx_compiler PROPERTIES EXPORT_NAME compiler)
add_library(nsx::compiler ALIAS nsx_compiler)
target_link_libraries(nsx_compiler INTERFACE nsx_core_includes)

# ---- nsx::util — byte ring buffer + safe-memcpy helpers ----
add_library(nsx_util STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_ring_buffer.c
)
set_target_properties(nsx_util PROPERTIES EXPORT_NAME util)
add_library(nsx::util ALIAS nsx_util)
target_link_libraries(nsx_util PUBLIC nsx_core_includes nsx::soc_hal)

# ---- nsx::mem — memory-placement macros + I/D cache control ----
add_library(nsx_mem STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_cache.c
)
set_target_properties(nsx_mem PROPERTIES EXPORT_NAME mem)
add_library(nsx::mem ALIAS nsx_mem)
target_link_libraries(nsx_mem PUBLIC nsx_core_includes nsx::compiler nsx::soc_hal)

# ---- nsx::core_lifecycle — runtime init + API versioning ----
add_library(nsx_core_lifecycle STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_core_lifecycle.c
)
set_target_properties(nsx_core_lifecycle PROPERTIES EXPORT_NAME core_lifecycle)
add_library(nsx::core_lifecycle ALIAS nsx_core_lifecycle)
target_link_libraries(nsx_core_lifecycle PUBLIC nsx_core_includes nsx::compiler nsx::soc_hal)

# ---- nsx::system — bring-up sequencing + per-SoC backend + newlib retarget/heap ----
set(NSX_SYSTEM_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_system.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_retarget.c
)
if(NSX_CORE_ARCH_DIR)
    list(APPEND NSX_SYSTEM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_CORE_ARCH_DIR}/nsx_system_platform.c
    )
endif()

# Bounded _sbrk for newlib-based toolchains (GCC, ATfE).  armclang uses
# its own ARM_LIB_HEAP / ARM_LIB_STACK runtime and does not need this.
nsx_toolchain_uses_newlib(NSX_TOOLCHAIN_USES_NEWLIB)
if(NSX_TOOLCHAIN_USES_NEWLIB)
    list(APPEND NSX_SYSTEM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/sbrk.c
    )
endif()

add_library(nsx_system STATIC
    ${NSX_SYSTEM_SOURCES}
)
set_target_properties(nsx_system PROPERTIES EXPORT_NAME system)
add_library(nsx::system ALIAS nsx_system)
target_link_libraries(nsx_system PUBLIC
    nsx_core_includes
    nsx::core_lifecycle
    nsx::mem
    nsx::soc_hal
)

# The newlib retarget stubs (nsx_retarget.c) define only `__wrap__*` symbols,
# which the toolchain references via `-Wl,--wrap=...`. Because nothing else
# pulls that object out of the static archive, force the linker to retain it
# with matching `-u` (undefined-symbol) flags. Gated to newlib toolchains —
# the stubs are compiled only when NSX_HAS_NEWLIB is set.
if(NSX_TOOLCHAIN_USES_NEWLIB)
    target_link_options(nsx_system PUBLIC
        -Wl,-u,__wrap__write_r
        -Wl,-u,__wrap__close_r
        -Wl,-u,__wrap__lseek_r
        -Wl,-u,__wrap__read_r
        -Wl,-u,__wrap__kill_r
        -Wl,-u,__wrap__getpid_r
        -Wl,-u,__wrap__fstat_r
        -Wl,-u,__wrap__isatty_r
    )
endif()

# ---- nsx::presets — opinionated, opt-in system presets (NOT in nsx::core) ----
# Convenience configs for quick bring-up, demos, and tools. Link explicitly
# and include "nsx_presets.h" to use.
add_library(nsx_presets STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_presets.c
)
set_target_properties(nsx_presets PROPERTIES EXPORT_NAME presets)
add_library(nsx::presets ALIAS nsx_presets)
target_link_libraries(nsx_presets PUBLIC nsx_core_includes)

# ---- nsx::runtime — printf / delay / interrupt / debug shims ----
add_library(nsx_runtime STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_core_portable.c
)
set_target_properties(nsx_runtime PROPERTIES EXPORT_NAME runtime)
add_library(nsx::runtime ALIAS nsx_runtime)
target_link_libraries(nsx_runtime PUBLIC
    nsx_core_includes
    nsx::core_lifecycle
    nsx::system
    nsx::soc_hal
)

# ---- nsx::core — backward-compatible umbrella over every sub-library ----
add_library(nsx_core INTERFACE)
set_target_properties(nsx_core PROPERTIES EXPORT_NAME core)
add_library(nsx::core ALIAS nsx_core)
target_link_libraries(nsx_core INTERFACE
    nsx::compiler
    nsx::util
    nsx::mem
    nsx::core_lifecycle
    nsx::runtime
    nsx::system
)

add_library(nsx_runtime_core INTERFACE)
set_target_properties(nsx_runtime_core PROPERTIES EXPORT_NAME runtime_core)
add_library(nsx::runtime_core ALIAS nsx_runtime_core)
add_library(nsx::core_min ALIAS nsx_runtime_core)
target_link_libraries(nsx_runtime_core INTERFACE nsx_core)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core
)

install(TARGETS
    nsx_core_includes
    nsx_compiler
    nsx_util
    nsx_mem
    nsx_core_lifecycle
    nsx_system
    nsx_presets
    nsx_runtime
    nsx_core
    nsx_runtime_core
    EXPORT nsxTargets
)
