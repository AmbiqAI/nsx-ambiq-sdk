// Link-only C++ initialization probe. No board startup or MMIO executes.
#include <cstdlib>

extern "C" const unsigned g_ui32HALversion;
extern "C" void __libc_init_array();
extern "C" void _init() {}

void *volatile allocated;
volatile unsigned marker;
struct BeforeMain {
    BeforeMain() {
        allocated = malloc(32);
        marker = 1;
    }
} before_main;

extern "C" __attribute__((noinline)) int main() {
    // Extract am_hal_global.o through an ordinary HAL reference.
    marker = g_ui32HALversion;
    return allocated == nullptr;
}

extern "C" void _start() {
    __libc_init_array();
    main();
    for (;;) {}
}
