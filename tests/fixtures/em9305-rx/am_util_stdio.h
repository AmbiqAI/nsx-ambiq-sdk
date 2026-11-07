#ifndef TEST_EM9305_UTIL_H
#define TEST_EM9305_UTIL_H
#include <stdint.h>
void am_util_delay_us(uint32_t);
void am_util_debug_printf(const char *, ...);
void am_util_stdio_printf(const char *, ...);
#endif
