#ifndef TEST_EM9305_EVENT_GROUPS_H
#define TEST_EM9305_EVENT_GROUPS_H
#include "FreeRTOS.h"
typedef void *EventGroupHandle_t;
typedef unsigned EventBits_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t, EventBits_t, BaseType_t *);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t, BaseType_t, TickType_t);
#endif
