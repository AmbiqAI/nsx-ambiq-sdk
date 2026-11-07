#ifndef TEST_EM9305_FREERTOS_H
#define TEST_EM9305_FREERTOS_H
#include <assert.h>
#include <stdint.h>
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) (ms)
#define pdFALSE 0
#define pdTRUE 1
#define pdFAIL 0
#define portMAX_DELAY UINT32_MAX
#define portYIELD() ((void)0)
#define portYIELD_FROM_ISR(x) ((void)(x))
typedef int BaseType_t;
BaseType_t xPortIsInsideInterrupt(void);
#define pdPASS 1
#define configASSERT(expr) assert(expr)
typedef uint32_t TickType_t;
TickType_t xTaskGetTickCount(void);
#endif
