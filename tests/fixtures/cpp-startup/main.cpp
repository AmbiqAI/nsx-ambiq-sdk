// A debugger can inspect constructor_result after main starts: 0x600DC0DE
// means the constructor observed initialized .data, .bss and (on Apollo3P)
// .tcm_bss. No HAL initialization is needed for this startup-only check.
volatile unsigned initialized_data = 0x12345678;
volatile unsigned zeroed_bss;
#if defined(AM_PART_APOLLO3P)
__attribute__((section(".tcm_bss"))) volatile unsigned zeroed_tcm_bss;
#endif
volatile unsigned constructor_flag;
volatile unsigned constructor_result;
volatile unsigned constructor_order;

extern "C" void preinit_probe(void) {
    constructor_order = 0xABC;
}
__attribute__((used, section(".preinit_array")))
void (*const preinit_hook)(void) = preinit_probe;

extern "C" __attribute__((constructor(101))) void priority_first(void) {
    constructor_order = constructor_order == 0xABC ? 1 : 0xBAD;
}

extern "C" __attribute__((constructor(1000))) void priority_second(void) {
    constructor_order = constructor_order == 1 ? 2 : 0xBAD;
}

struct Probe {
    Probe() {
        if (constructor_order == 2 && initialized_data == 0x12345678 && zeroed_bss == 0
#if defined(AM_PART_APOLLO3P)
            && zeroed_tcm_bss == 0
#endif
        ) {
            constructor_flag = 0x600DC0DE;
        }
    }
};
Probe global_probe;

int main(void) {
    constructor_result = constructor_flag == 0x600DC0DE ? 0x600DC0DE : 0xBAD;
    for (;;) {
        __asm volatile("nop");
    }
}
