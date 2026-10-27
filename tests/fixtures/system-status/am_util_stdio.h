#pragma once
#include <stdarg.h>
typedef void (*am_util_stdio_print_char_t)(char*);
void am_util_stdio_vprintf(const char*,va_list);
void am_util_stdio_printf_init(am_util_stdio_print_char_t);
