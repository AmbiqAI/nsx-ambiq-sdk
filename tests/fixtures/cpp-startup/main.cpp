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

struct Probe {
    Probe() {
        if (initialized_data == 0x12345678 && zeroed_bss == 0
#if defined(AM_PART_APOLLO3P)
            && zeroed_tcm_bss == 0
#endif
        ) {
            constructor_flag = 0x600DC0DE;
        }
    }
};
Probe global_probe;

// GCC newlib calls _init from __libc_init_array. This bare-metal fixture
// supplies it because -nostartfiles deliberately omits the CRT entry point.
extern "C" void _init(void) {}

int main(void) {
    constructor_result = constructor_flag == 0x600DC0DE ? 0x600DC0DE : 0xBAD;
    for (;;) {
        __asm volatile("nop");
    }
}
