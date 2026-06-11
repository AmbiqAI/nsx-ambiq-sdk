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