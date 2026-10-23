#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "am_mcu_apollo.h"
#include "unity.h"
#include "result_reporter.h"

extern unsigned char __HeapBase, __HeapLimit;
extern unsigned char __nsx_test_stack_start, __nsx_test_stack_end;
extern void *_sbrk(ptrdiff_t increment);
static void *allocations[2];
static volatile uint32_t svc_count, svc_exception;

void setUp(void) { allocations[0] = allocations[1] = NULL; }
void tearDown(void) { free(allocations[0]); free(allocations[1]); }

/* Sample at an ABI call boundary, before a compiler-generated stack frame. */
__attribute__((naked, noinline)) static uint32_t entry_msp(void) {
    __asm volatile("mrs r0, msp\n bx lr");
}

static int within(uintptr_t address, size_t size, uintptr_t start, uintptr_t end) {
    return start <= address && address <= end && size <= end - address;
}

static void stack_bounds_alignment(void) {
    uintptr_t low = (uintptr_t)&__nsx_test_stack_start;
    uintptr_t high = (uintptr_t)&__nsx_test_stack_end;
    uintptr_t sp = entry_msp();
    volatile uint32_t local = 0;
    TEST_ASSERT_TRUE(high > low);
    TEST_ASSERT_EQUAL_UINT32(0, sp % 8U);
    TEST_ASSERT_TRUE(sp >= low && sp <= high);
    TEST_ASSERT_TRUE(within((uintptr_t)&local, sizeof(local), low, high));
}

static void heap_allocation(void) {
    uintptr_t low = (uintptr_t)&__HeapBase;
    uintptr_t high = (uintptr_t)&__HeapLimit;
    uintptr_t stack_low = (uintptr_t)&__nsx_test_stack_start;
    uintptr_t stack_high = (uintptr_t)&__nsx_test_stack_end;
    TEST_ASSERT_TRUE(high > low);
    TEST_ASSERT_TRUE(high <= stack_low || low >= stack_high);
    allocations[0] = malloc(64);
    allocations[1] = malloc(96);
    TEST_ASSERT_NOT_NULL(allocations[0]);
    TEST_ASSERT_NOT_NULL(allocations[1]);
    TEST_ASSERT_TRUE(within((uintptr_t)allocations[0], 64, low, high));
    TEST_ASSERT_TRUE(within((uintptr_t)allocations[1], 96, low, high));
    TEST_ASSERT_EQUAL_UINT32(0, (uintptr_t)allocations[0] % _Alignof(max_align_t));
    TEST_ASSERT_EQUAL_UINT32(0, (uintptr_t)allocations[1] % _Alignof(max_align_t));
    volatile uint8_t *a = allocations[0], *b = allocations[1];
    for (unsigned i = 0; i < 64; ++i) a[i] = (uint8_t)(i ^ 0xA5U);
    for (unsigned i = 0; i < 96; ++i) b[i] = (uint8_t)(i ^ 0x5AU);
    for (unsigned i = 0; i < 64; ++i) TEST_ASSERT_EQUAL_HEX8(i ^ 0xA5U, a[i]);
    for (unsigned i = 0; i < 96; ++i) TEST_ASSERT_EQUAL_HEX8(i ^ 0x5AU, b[i]);
}

static void heap_limit_rejection(void) {
    uintptr_t current = (uintptr_t)_sbrk(0);
    uintptr_t low = (uintptr_t)&__HeapBase, high = (uintptr_t)&__HeapLimit;
    TEST_ASSERT_TRUE(current >= low && current <= high);
    size_t excess = high - current + 1U;
    TEST_ASSERT_TRUE(excess <= PTRDIFF_MAX);
    errno = 0;
    void *result = _sbrk((ptrdiff_t)excess);
    int error = errno;
    uintptr_t after = (uintptr_t)_sbrk(0);
    TEST_ASSERT_EQUAL_PTR((void *)-1, result);
    TEST_ASSERT_EQUAL_INT(ENOMEM, error);
    TEST_ASSERT_EQUAL_HEX32(current, after);
}

void SVC_Handler(void) {
#ifndef NSX_TEST_NO_SVC_OBSERVATION
    ++svc_count;
    svc_exception = __get_IPSR();
#endif
}

static void svc_dispatch(void) {
    /* CMSIS exception numbers are relative to the 16-entry core vector prefix. */
    const unsigned slot = (unsigned)(16 + SVCall_IRQn);
    const uintptr_t *vectors = (const uintptr_t *)SCB->VTOR;
    TEST_ASSERT_EQUAL_HEX32((uintptr_t)SVC_Handler, vectors[slot]);
    uint32_t before = svc_count;
    uint32_t primask = __get_PRIMASK(), basepri = __get_BASEPRI();
    __set_BASEPRI(0);
    __enable_irq();
    __asm volatile("svc 0" ::: "memory");
    __disable_irq();
    __set_BASEPRI(basepri);
    __set_PRIMASK(primask);
    TEST_ASSERT_EQUAL_UINT32(before + 1U, svc_count);
    TEST_ASSERT_EQUAL_UINT32(slot, svc_exception);
}

int main(void) {
    nsx_test_begin(__FILE__);
    RUN_TEST(stack_bounds_alignment);
    RUN_TEST(heap_allocation);
    RUN_TEST(heap_limit_rejection);
    RUN_TEST(svc_dispatch);
    nsx_test_finish();
}
