# nsx_load_soc_facts(<skew>)
#
# Publish the side-effect-free SoC facts for <skew> into the *caller's* scope:
#   NSX_SOC_FAMILY / NSX_SOC_SERIES / NSX_SOC_SKEW / NSX_SOC_CORE /
#   NSX_SOC_ARCH_CLASS / NSX_SOC_SDK_PROVIDER / NSX_SOC_HAS_{DSP,MVE,FPU} /
#   NSX_SOC_PMU_TIER / NSX_SOC_CAPABILITIES /
#   NSX_SOC_RTOS_PORT_FAMILY / NSX_SOC_RTOS_PORT_GENERIC
# and the NSX toolchain selectors NSX_CPU / NSX_FLOAT_ABI / NSX_ABI_FLAGS.
#
# This is the single source of truth shared by the SDK's own
# cmake/socs/<skew>.cmake descriptors and by downstream board.cmake files
# (e.g. neuralspotx boards), so a board never re-declares these facts by hand
# and they cannot drift between the two build paths.
#
# Implemented as a macro (not a function) so that include() sets the facts
# directly in the calling scope.

set(_NSX_SOC_FACTS_DIR "${CMAKE_CURRENT_LIST_DIR}/socs/facts")

macro(nsx_load_soc_facts _nsx_soc_skew)
    if(NOT EXISTS "${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake")
        message(FATAL_ERROR
            "nsx_load_soc_facts: unknown SoC skew '${_nsx_soc_skew}' "
            "(no ${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake)")
    endif()
    include("${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake")
endmacro()

# nsx_soc_flags_target(<target>)
#
# Create an INTERFACE library <target> that carries the SoC compile
# definitions published by nsx_load_soc_facts() in NSX_SOC_COMPILE_DEFINITIONS,
# each gated to C (so assembly startup and the link step are unaffected), and
# alias it as nsx::soc_flags.
#
# This is the single point where the SoC define list becomes compiler flags. It
# is shared by the SDK's own cmake/socs/<skew>.cmake descriptors and by
# downstream board.cmake files (e.g. neuralspotx boards), so the SoC details
# (PART/AM_PART/core/capability macros) are owned by the SoC layer and never
# re-declared by a board.
#
# Idempotent: returns early if <target> already exists. Requires
# nsx_load_soc_facts(<skew>) to have been called first.
function(nsx_soc_flags_target _nsx_soc_flags_target)
    if(TARGET ${_nsx_soc_flags_target})
        return()
    endif()
    if(NOT DEFINED NSX_SOC_COMPILE_DEFINITIONS)
        message(FATAL_ERROR
            "nsx_soc_flags_target(${_nsx_soc_flags_target}): "
            "NSX_SOC_COMPILE_DEFINITIONS is not set. "
            "Call nsx_load_soc_facts(<skew>) first.")
    endif()
    add_library(${_nsx_soc_flags_target} INTERFACE)
    foreach(_nsx_soc_def IN LISTS NSX_SOC_COMPILE_DEFINITIONS)
        target_compile_definitions(${_nsx_soc_flags_target} INTERFACE
            $<$<COMPILE_LANGUAGE:C>:${_nsx_soc_def}>)
    endforeach()
    if(NOT TARGET nsx::soc_flags)
        add_library(nsx::soc_flags ALIAS ${_nsx_soc_flags_target})
    endif()
endfunction()
