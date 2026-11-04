/* Run the shipped WSF queue implementation; replace only RTOS scheduling.
 * Start/stop are renamed so main.c can count calls around the real operations. */
#define WsfTimerStartMs real_WsfTimerStartMs
#define WsfTimerStop real_WsfTimerStop
#include "../../../modules/nsx-cordio/sdk/third_party/cordio/wsf/sources/port/freertos/wsf_timer.c"
void WsfTaskLock(void) {}
void WsfTaskUnlock(void) {}
void WsfTaskSetReady(wsfHandlerId_t handler, wsfTaskEvent_t event) {
    (void)handler;
    (void)event;
}
#include "wsf_cs.h"
void WsfCsEnter(void) {}
void WsfCsExit(void) {}
