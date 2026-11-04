#ifndef TEST_BLE_TIMERS_H
#define TEST_BLE_TIMERS_H
#include "FreeRTOS.h"
typedef void *TimerHandle_t;
TimerHandle_t xTimerCreate(const char *, TickType_t, int, void *, void (*)(TimerHandle_t));
int xTimerChangePeriod(TimerHandle_t, TickType_t, TickType_t);
#endif
