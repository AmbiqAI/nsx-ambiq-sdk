#ifndef TEST_BLE_FREERTOS_H
#define TEST_BLE_FREERTOS_H
#include <assert.h>
#include <stdint.h>
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) (ms)
#define pdFALSE 0
#define pdPASS 1
#define configASSERT(expr) assert(expr)
typedef uint32_t TickType_t;
TickType_t xTaskGetTickCount(void);
#endif
