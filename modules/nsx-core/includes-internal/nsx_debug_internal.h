#ifndef NSX_DEBUG_INTERNAL_H
#define NSX_DEBUG_INTERNAL_H
#include "nsx_system.h"

bool nsx_debug_output_enabled(void);
void nsx_debug_before_hw_init(void);
void nsx_debug_resume(void);
void nsx_debug_disable(nsx_debug_transport_e transport);
void nsx_debug_suspend(void);
#endif
