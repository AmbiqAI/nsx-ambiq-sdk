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

set(_NSX_SOC_FACTS_DIR "${CMAKE_CURRENT_LIST_DIR}/socs/_facts")

macro(nsx_load_soc_facts _nsx_soc_skew)
    if(NOT EXISTS "${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake")
        message(FATAL_ERROR
            "nsx_load_soc_facts: unknown SoC skew '${_nsx_soc_skew}' "
            "(no ${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake)")
    endif()
    include("${_NSX_SOC_FACTS_DIR}/${_nsx_soc_skew}.cmake")
endmacro()
