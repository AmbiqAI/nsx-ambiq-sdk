#include <stdint.h>
#include "unity.h"
#include "identity.h"
#include "result_reporter.h"

struct Result {
    uint32_t magic, version, state, total, failed, ignored, length, overflow;
    char identity[64];
    char text[4096];
};
extern "C" {
volatile Result nsx_test_result;
void nsx_test_putchar(int ch) {
    uint32_t n = nsx_test_result.length;
    if (n < sizeof(nsx_test_result.text)) {
        nsx_test_result.text[n] = (char)ch;
        nsx_test_result.length = n + 1;
    } else {
        nsx_test_result.overflow = 1;
    }
}
__attribute__((noreturn)) void _exit(int) {
    nsx_test_result.state = 3;
    for (;;) __asm volatile("nop");
}
}
void nsx_test_begin(void) {
    nsx_test_result.version = 1;
    const char id[] = NSX_TEST_BUILD_ID;
    for (unsigned i = 0; i < sizeof(id); ++i) nsx_test_result.identity[i] = id[i];
    nsx_test_result.state = 1;
    __asm volatile("dmb" ::: "memory");
    nsx_test_result.magic = 0x4E535854;
    UNITY_BEGIN();
}

void nsx_test_finish(void) {
    UNITY_END();
    nsx_test_result.total = Unity.NumberOfTests;
    nsx_test_result.failed = Unity.TestFailures;
    nsx_test_result.ignored = Unity.TestIgnores;
    __asm volatile("dsb" ::: "memory");
#ifdef NSX_TEST_NO_COMPLETION
    for (;;) __asm volatile("nop");
#endif
    nsx_test_result.state = 2;
    __asm volatile("dsb" ::: "memory");
    for (;;) __asm volatile("nop");
}
