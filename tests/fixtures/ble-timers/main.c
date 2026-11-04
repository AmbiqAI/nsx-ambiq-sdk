/* Include the entire production translation unit, retaining real BLE/Cordio
 * types. This permits fixture setup of currentConnId without emulating radio
 * startup or changing production visibility. Only external boundaries are fake. */
#include "platform.h"
#include "../../../modules/nsx-ble/src/ns_ble.c"
#include <assert.h>
#include <stdio.h>

static unsigned starts[3], stops[3], callbacks[3], sent, sets, events, ccc_queries;
static uint16_t periods[3], subscribed[3], sent_handle;
static uint8_t sent_value, queried_index;
static int callback_status;
static bool security_pending;
static unsigned callback_action, ccc_action;
static void subscription(ns_ble_characteristic_t *, uint16_t);
static void disconnect(void);
static ns_ble_characteristic_t sync_char, async_char;
static ns_ble_characteristic_t *characteristics[2];
static attsAttr_t attributes[7];
static attsCccSet_t ccc[3];
static ns_ble_service_control_t control;
static ns_ble_service_t service;
static uint8_t sync_value = 41, async_value = 73;

void ns_lp_printf(const char *format, ...) { (void)format; }
void ns_interrupt_master_disable(void) {}
void ns_interrupt_master_enable(void) {}
void WsfTimerStartMs(wsfTimer_t *timer, wsfTimerTicks_t period) {
    unsigned index = timer->msg.status;
    assert(index > 0 && index < 3);
    starts[index]++;
    periods[index] = period;
#ifdef NSX_TEST_REAL_WSF_TIMER
    extern void real_WsfTimerStartMs(wsfTimer_t *, wsfTimerTicks_t);
    real_WsfTimerStartMs(timer, period);
#else
    timer->isStarted = true;
#endif
}
void WsfTimerStop(wsfTimer_t *timer) {
    unsigned index = timer->msg.status;
    assert(index > 0 && index < 3);
    stops[index]++;
#ifdef NSX_TEST_REAL_WSF_TIMER
    extern void real_WsfTimerStop(wsfTimer_t *);
    real_WsfTimerStop(timer);
#else
    timer->isStarted = false;
#endif
}
uint16_t AttsCccGet(dmConnId_t connection, uint8_t index) {
    assert(connection == 1 && index > 0 && index < 3);
    ccc_queries++;
    queried_index = index;
    return subscribed[index];
}
uint16_t AttsCccEnabled(dmConnId_t connection, uint8_t index) {
    return security_pending ? 0 : AttsCccGet(connection, index);
}
uint8_t AttsSetAttr(uint16_t handle, uint16_t length, uint8_t *value) {
    assert(length == 1 && value != NULL);
    sets++;
    sent_handle = handle;
    sent_value = *value;
    return ATT_SUCCESS;
}
void AttsHandleValueNtf(dmConnId_t connection, uint16_t handle, uint16_t length,
                       uint8_t *value) {
    assert(connection == 1 && length == 1 && handle == sent_handle);
    assert(*value == sent_value);
    sent++;
}
static int notify(ns_ble_service_t *owner, ns_ble_characteristic_t *characteristic) {
    assert(owner == &service);
    callbacks[characteristic->cccIndex]++;
    ++*(uint8_t *)characteristic->applicationValue;
    if (callback_action == 1) subscription(characteristic, 0);
    if (callback_action == 2) disconnect();
    return callback_status;
}
static void event(const ns_ble_event_t *message, void *context) {
    assert(context == &service && message->type == NS_BLE_EVENT_CCC_CHANGED);
    assert(message->value0 > 0 && message->value0 < 3);
    events++;
    if (ccc_action) {
        unsigned action = ccc_action; ccc_action = 0;
        if (action == 1) subscription(&sync_char, 0);
        else disconnect();
    }
}
static void setup(void) {
    service.numCharacteristics = 2;
    service.attributes = attributes;
    service.characteristics = characteristics;
    service.cccSet = ccc;
    service.nextAttributeIndex = 1;
    service.nextHandleId = 0x20;
    service.nextCccIndex = 1; /* GATT Service Changed reserves index zero. */
    service.nextCccIndicationHandle = 0xc0;
    control.eventHandler = event;
    control.eventContext = &service;
    g_ns_ble_control.service_config = &control;
    g_ns_ble_control.numServices = 1;
    g_ns_ble_control.services[0] = &service;
    g_ns_ble_control.handlerId = 7;
    currentConnId = 1;
    callback_status = NS_STATUS_SUCCESS;
    assert(ns_ble_create_characteristic(&sync_char, "00000000000000000000000000000001",
        &sync_value, 1, NS_BLE_NOTIFY, NULL, NULL, notify, 200, 0,
        &service.numAttributes) == NS_STATUS_SUCCESS);
    assert(ns_ble_create_characteristic(&async_char, "00000000000000000000000000000002",
        &async_value, 1, NS_BLE_NOTIFY, NULL, NULL, notify, 350, 2,
        &service.numAttributes) == NS_STATUS_SUCCESS);
    assert(ns_ble_add_characteristic(&service, &sync_char) == NS_STATUS_SUCCESS);
    assert(ns_ble_add_characteristic(&service, &async_char) == NS_STATUS_SUCCESS);
    assert(service.numAttributes == 6 && service.nextAttributeIndex == 7);
    assert(sync_char.cccIndex == 1 && async_char.cccIndex == 2);
    assert(async_char.indicationTimer.msg.status == 2);
    assert(async_char.indicationTimer.handlerId == 7);
    assert(async_char.indicationTimer.msg.event == async_char.cccIndicationHandle);
    assert(ccc[2].handle == async_char.cccHandle);
}
static void subscription(ns_ble_characteristic_t *characteristic, uint16_t value) {
    ns_ble_msg_t message = {0};
    message.ccc.hdr.event = ATTS_CCC_STATE_IND;
    message.ccc.hdr.param = 1;
    message.ccc.idx = characteristic->cccIndex;
    message.ccc.value = value;
    subscribed[characteristic->cccIndex] = value;
    assert(ns_ble_new_proc_msg(&message));
}
static void disconnect(void) {
    ns_ble_msg_t message = {0};
    message.hdr.event = DM_CONN_CLOSE_IND;
    message.hdr.param = 1;
    assert(!ns_ble_new_proc_msg(&message)); /* Generic processing must continue. */
    /* Model the generic close and Cordio CCC cleanup; hardware exercises both. */
    currentConnId = DM_CONN_ID_NONE;
    subscribed[1] = subscribed[2] = 0;
}
static void stale_timer(ns_ble_characteristic_t *characteristic) {
#ifdef NSX_TEST_REAL_WSF_TIMER
    extern void real_WsfTimerStartMs(wsfTimer_t *, wsfTimerTicks_t);
    real_WsfTimerStartMs(&characteristic->indicationTimer, characteristic->indicationPeriod);
#else
    characteristic->indicationTimer.isStarted = true;
#endif
}
static void expiry(ns_ble_characteristic_t *characteristic) {
#ifdef NSX_TEST_REAL_WSF_TIMER
    if (characteristic->indicationTimer.isStarted) {
        WsfTimerUpdate(characteristic->indicationPeriod / WSF_MS_PER_TICK);
        assert(WsfTimerServiceExpired(0) == &characteristic->indicationTimer);
    }
#endif
    ns_ble_msg_t message = {0};
    message.hdr = characteristic->indicationTimer.msg;
    assert(ns_ble_new_proc_msg(&message));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    setup();
    if (!strcmp(argv[1], "async-enable")) {
        stale_timer(&async_char); /* Clean up a previously armed timer. */
        subscription(&async_char, ATT_CLIENT_CFG_NOTIFY);
        subscription(&async_char, ATT_CLIENT_CFG_NOTIFY);
        assert(events == 2 && starts[2] == 0 && stops[2] == 2);
        assert(!async_char.indicationTimer.isStarted && callbacks[2] == 0 && sent == 0);
    } else if (!strcmp(argv[1], "async-expiry")) {
        stale_timer(&async_char);
        expiry(&async_char); /* Matching stale expiry, independent of CCC enable. */
        assert(starts[2] == 0 && callbacks[2] == 0 && sent == 0 && stops[2] == 1);
        assert(!async_char.indicationTimer.isStarted);
    } else if (!strcmp(argv[1], "explicit-send")) {
        subscription(&async_char, ATT_CLIENT_CFG_NOTIFY);
        assert(ns_ble_send_value(&async_char, NULL) == NS_STATUS_SUCCESS);
        assert(sent == 1 && sets == 1 && queried_index == 2 && sent_value == async_value);
        assert(sent_handle == async_char.valueHandle && starts[2] == 0);
        subscription(&async_char, 0);
        assert(ns_ble_send_value(&async_char, NULL) == NS_STATUS_FAILURE);
        assert(sent == 1 && sets == 1);
        currentConnId = DM_CONN_ID_NONE;
        subscribed[2] = ATT_CLIENT_CFG_NOTIFY;
        unsigned before = ccc_queries;
        assert(ns_ble_send_value(&async_char, NULL) == NS_STATUS_FAILURE);
        assert(ccc_queries == before && sent == 1 && sets == 1);
    } else if (!strcmp(argv[1], "disconnect") || !strcmp(argv[1], "queue-disconnect")) {
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
#ifdef NSX_TEST_REAL_WSF_TIMER
        WsfTimerUpdate(20); /* Already expired, still queued when close is processed. */
#endif
        disconnect();
        assert(!sync_char.indicationTimer.isStarted && stops[1] == 1);
#ifdef NSX_TEST_REAL_WSF_TIMER
        assert(WsfTimerServiceExpired(0) == NULL);
#endif
        expiry(&sync_char);
        assert(callbacks[1] == 0 && starts[1] == 1 && sent == 0);
        currentConnId = 1;
        expiry(&sync_char);
        assert(callbacks[1] == 0 && starts[1] == 1);
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        expiry(&sync_char);
        assert(callbacks[1] == 1 && starts[1] == 3 && sent == 1);
#ifdef NSX_TEST_REAL_WSF_TIMER
        WsfTimerUpdate(20);
        assert(WsfTimerServiceExpired(0) == &sync_char.indicationTimer);
        assert(WsfTimerServiceExpired(0) == NULL);
#endif
    } else if (!strncmp(argv[1], "bonded-", 7)) {
        /* ATT restores CCCs before the application's connection-open message. */
        currentConnId = DM_CONN_ID_NONE;
        security_pending = strstr(argv[1], "security") != NULL;
        if (strstr(argv[1], "disable")) ccc_action = 1;
        if (strstr(argv[1], "disconnect")) ccc_action = 2;
        bool cancelled = ccc_action != 0;
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        subscription(&async_char, ATT_CLIENT_CFG_NOTIFY);
        assert(starts[2] == 0 && !async_char.indicationTimer.isStarted);
        if (cancelled) {
            assert(starts[1] == 0 && !sync_char.indicationTimer.isStarted);
        } else {
            assert(starts[1] == 1 && sync_char.indicationTimer.isStarted);
            currentConnId = 1; /* Generic connection-open processing. */
            expiry(&sync_char);
            assert(callbacks[1] == 1 && sent == (security_pending ? 0u : 1u) && starts[1] == 2);
            if (security_pending) {
                security_pending = false;
                expiry(&sync_char);
                assert(callbacks[1] == 2 && sent == 1 && starts[1] == 3);
            }
        }
    } else if (!strcmp(argv[1], "unsubscribed-expiry")) {
        expiry(&sync_char);
        assert(callbacks[1] == 0 && starts[1] == 0 && sent == 0);
    } else if (!strncmp(argv[1], "callback-", 9)) {
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        callback_action = strstr(argv[1], "disconnect") ? 2 : 1;
        if (strstr(argv[1], "failure")) callback_status = NS_STATUS_FAILURE;
        expiry(&sync_char);
        assert(callbacks[1] == 1 && starts[1] == 1 && sent == 0);
        assert(!sync_char.indicationTimer.isStarted);
    } else if (!strncmp(argv[1], "ccc-callback-", 13)) {
        ccc_action = strstr(argv[1], "disconnect") ? 2 : 1;
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        assert(starts[1] == 0 && !sync_char.indicationTimer.isStarted);
    } else if (!strncmp(argv[1], "sync-", 5)) {
        bool null_callback = !strcmp(argv[1], "sync-null");
        bool failure = !strcmp(argv[1], "sync-failure");
        if (null_callback) sync_char.notifyHandlerCb = NULL;
        if (failure) callback_status = NS_STATUS_FAILURE;
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        expiry(&sync_char);
        assert(starts[1] == 2 && periods[1] == 200 && stops[1] == 0);
        assert(callbacks[1] == (null_callback ? 0u : 1u));
        assert(sent == (failure ? 0u : 1u) && sets == sent);
        if (!failure) assert(sent_value == (null_callback ? 41 : 42));
    } else if (!strcmp(argv[1], "mixed-disable")) {
        subscription(&sync_char, ATT_CLIENT_CFG_NOTIFY);
        subscription(&async_char, ATT_CLIENT_CFG_NOTIFY);
        expiry(&sync_char);
        expiry(&async_char);
        assert(starts[1] == 2 && starts[2] == 0 && callbacks[1] == 1 && callbacks[2] == 0);
        assert(sent == 1 && events == 2);
        subscription(&async_char, 0);
        assert(sync_char.indicationTimer.isStarted);
        subscription(&sync_char, 0);
        assert(!sync_char.indicationTimer.isStarted && stops[1] == 1);
        assert(events == 4 && starts[1] == 2 && sent == 1);
    } else if (!strcmp(argv[1], "unknown")) {
        ns_ble_msg_t message = {0};
        message.hdr.event = 0xcf;
        assert(!ns_ble_new_proc_msg(&message));
        assert(starts[1] == 0 && starts[2] == 0 && callbacks[1] == 0 && callbacks[2] == 0);
        assert(sent == 0 && events == 0);
    } else {
        assert(!"unknown test scenario");
    }
    free(sync_char.pValue);
    free(async_char.pValue);
    return 0;
}
