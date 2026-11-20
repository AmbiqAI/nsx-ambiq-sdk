#pragma once

#include <stdint.h>

uint32_t am_hal_interrupt_master_disable(void);
void am_hal_interrupt_master_set(uint32_t mask);
