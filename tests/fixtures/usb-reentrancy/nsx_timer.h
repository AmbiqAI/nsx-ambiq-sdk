#pragma once
#include <stdbool.h>
#include <stdint.h>
#define NSX_TIMER_USB 1
extern const int nsx_timer_V1_0_0;
typedef struct nsx_timer_config nsx_timer_config_t;
struct nsx_timer_config { const int *api; unsigned timer; bool enableInterrupt; uint32_t periodInMicroseconds; void (*callback)(nsx_timer_config_t *); };
uint32_t nsx_timer_init(nsx_timer_config_t *cfg);
