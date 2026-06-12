/*
 * SPDX-License-Identifier: MIT
 *
 * Bridge the weak NSX startup vectors to the upstream GCC ARM_CM4F FreeRTOS
 * handler entry points. The CM4F port exposes vPortSVCHandler,
 * xPortPendSVHandler, and xPortSysTickHandler rather than strong
 * SVC_Handler/PendSV_Handler/SysTick_Handler symbols.
 */

void vPortSVCHandler(void);
void xPortPendSVHandler(void);
void xPortSysTickHandler(void);

/*
 * Referenced from nsx_freertos_start() so the linker pulls this translation
 * unit out of libnsx_freertos.a. Without an explicit reference, the strong
 * SVC_Handler / PendSV_Handler / SysTick_Handler symbols below would never be
 * extracted from the archive and the weak NSX startup stubs would remain in the
 * vector table.
 */
void nsx_freertos_force_cm4f_handlers(void)
{
}

void SVC_Handler(void) __attribute__((naked));
void PendSV_Handler(void) __attribute__((naked));

void SVC_Handler(void)
{
    __asm volatile("b vPortSVCHandler");
}

void PendSV_Handler(void)
{
    __asm volatile("b xPortPendSVHandler");
}

void SysTick_Handler(void)
{
    xPortSysTickHandler();
}
