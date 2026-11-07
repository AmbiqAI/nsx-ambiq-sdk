#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hci_drv_em9305.c"
extern volatile uint32_t g_ui32InterruptsSeenCopy;

static bool device_mode, ready = true, hold_ready, inject_irq;
static unsigned events, errors, last_error, reads, writes, resets, boots;
static unsigned mock_status, mock_length = 8, accept_limit = 1024;
static unsigned empty_before_success;
static bool drop_after_empty;
static uint8_t delivered[1024];
static unsigned delivered_length;
static unsigned chunks[3], chunk_count, chunk_index, payload_offset;
static unsigned headers, transfers, bus_enabled, bus_disabled;
static unsigned command_error, data_error, status_byte = 0xc0;
static am_devices_em9305_t device;

bool am_hal_queue_empty(am_hal_queue_t *q) { return q->count == 0; }
bool am_hal_queue_full(am_hal_queue_t *q) { return q->count == q->capacity; }
void *am_hal_queue_peek(am_hal_queue_t *q) { return (char *)q->buffer + q->head * q->size; }
void *am_hal_queue_next_slot(am_hal_queue_t *q) { return (char *)q->buffer + ((q->head+q->count)%q->capacity)*q->size; }
void am_hal_queue_item_add(am_hal_queue_t *q, void *p, unsigned n) { (void)p; assert(n == 1); q->count++; }
void am_hal_queue_item_get(am_hal_queue_t *q, void *p, unsigned n) { (void)p; assert(n == 1 && q->count); q->head=(q->head+1)%q->capacity; q->count--; }
uint32_t am_hal_gpio_input_read(unsigned pin) { assert(pin == GPIO_INT); return ready; }
void am_hal_gpio_state_write(unsigned pin, unsigned state) { assert(pin == AM_BSP_GPIO_EM9305_CS); (void)state; }
void am_hal_gpio_interrupt_register(unsigned a, unsigned b, am_hal_gpio_handler_t c, void *d) { (void)a;(void)b;(void)c;(void)d; }
void am_hal_mcuctrl_info_get(unsigned a, void *p) { (void)a; memset(p, 0, sizeof(am_hal_mcuctrl_device_t)); }
void NVIC_EnableIRQ(unsigned a) { (void)a; }
void am_util_delay_us(uint32_t us) { (void)us; }
void am_util_debug_printf(const char *s, ...) { (void)s; }
void am_util_stdio_printf(const char *s, ...) { (void)s; }
void WsfSetEvent(wsfHandlerId_t id, wsfEventMask_t e) { assert(id == 7 && e == BLE_TRANSFER_NEEDED_EVENT); events++; }
void WsfTimerStartMs(wsfTimer_t *t, wsfTimerTicks_t ms) { (void)t; (void)ms; }
void WsfTimerStop(wsfTimer_t *t) { (void)t; }
void HciReadLocalVerInfoCmd(void) { assert(0); }
void DmDevReset(void) { resets++; }
uint16_t hciTrSerialRxIncoming(uint8_t *p, uint16_t n) {
    unsigned accepted = n < accept_limit ? n : accept_limit;
    assert(delivered_length + accepted <= sizeof(delivered));
    memcpy(delivered + delivered_length, p, accepted); delivered_length += accepted;
    return accepted;
}
static void on_error(uint32_t e) { errors++; last_error=e; }
uint32_t am_devices_em9305_init(uint32_t m, am_devices_em9305_config_t *c, void **h, void **i) {
    (void)m; (void)c; *h=&device; *i=&device; boots++; return 0;
}
uint32_t am_devices_em9305_term(void *h) { (void)h; return 0; }
void am_devices_em9305_enable_interrupt(void) {}
void am_devices_em9305_disable_interrupt(void) {}
void am_devices_em9305_hsclk_req(bool e) { (void)e; }
void am_devices_em9305_disable(void) {}
void am_devices_em9305_request_counter_set(bool e) { (void)e; }
uint32_t am_devices_em9305_request_counter_get(void) { return 0; }
uint32_t am_devices_em9305_block_write(void *h, uint8_t *p, uint32_t n) {
    (void)h; assert(n == 9 && p[0] == 1); for(unsigned i=1;i<n;i++) assert(p[i]==0x42);
    writes++; return 0;
}
uint32_t am_devices_em9305_bus_enable(void *h) { assert(h == &device); bus_enabled++; return 0; }
uint32_t am_devices_em9305_bus_disable(void *h) { assert(h == &device); bus_disabled++; return 0; }
bool am_devices_em9305_spi_bitbang_enabled(void) { return false; }
void SPI_write_byte(uint8_t b) { (void)b; assert(0); }
uint8_t SPI_read_byte(void) { assert(0); return 0; }
uint32_t am_hal_iom_spi_blocking_fullduplex(void *h, am_hal_iom_transfer_t *t) {
    (void)h; assert(t->ui32NumBytes == 2 && t->eDirection == AM_HAL_IOM_FULLDUPLEX);
    assert(((uint8_t *)t->pui32TxBuffer)[0] == 0x81);
    assert(chunk_index < chunk_count); headers++;
    ((uint8_t *)t->pui32RxBuffer)[0] = status_byte;
    ((uint8_t *)t->pui32RxBuffer)[1] = chunks[chunk_index];
    if (inject_irq && chunk_index == 1) { HciDrvIntService(); inject_irq=false; }
    return command_error;
}
uint32_t am_hal_iom_blocking_transfer(void *h, am_hal_iom_transfer_t *t) {
    (void)h; assert(t->eDirection == AM_HAL_IOM_RX);
    assert(t->ui32NumBytes == chunks[chunk_index]); transfers++;
    if(data_error) return data_error;
    for(unsigned i=0;i<t->ui32NumBytes;i++) ((uint8_t *)t->pui32RxBuffer)[i]=(uint8_t)(payload_offset+i);
    payload_offset += t->ui32NumBytes;
    chunk_index++;
    ready=chunk_index < chunk_count;
    return 0;
}
uint32_t __real_am_devices_em9305_block_read(void *, uint32_t *, uint32_t *);
uint32_t __wrap_am_devices_em9305_block_read(void *h, uint32_t *p, uint32_t *n) {
    reads++;
    if(device_mode) return __real_am_devices_em9305_block_read(h,p,n);
    if(reads <= empty_before_success) {
        if(drop_after_empty && reads==empty_before_success) ready=false;
        return AM_DEVICES_EM9305_NO_DATA_TX;
    }
    if(mock_status == 0) {
        for(unsigned i=0;i<mock_length;i++) ((uint8_t *)p)[i]=(uint8_t)i;
        *n=mock_length;
    }
    if(!hold_ready) ready=false;
    return mock_status;
}
static void enqueue(void) { uint8_t p[8]; memset(p,0x42,sizeof(p)); assert(hciDrvWrite(1,sizeof(p),p)==sizeof(p)); events=0; }
static void check_bytes(unsigned n) { assert(delivered_length==n); for(unsigned i=0;i<n;i++) assert(delivered[i]==(uint8_t)i); }
static void handler(void) { HciDrvHandler(BLE_TRANSFER_NEEDED_EVENT,NULL); }

int main(int argc, char **argv) {
    assert(argc == 2); const char *s=argv[1];
    HciDrvHandlerInit(7); HciDrvErrorHandlerSet(on_error); HciDrvEmptyWriteQueue(); g_IomDevHdl=&device;
    if(!strncmp(s,"rx",2)) {
        mock_length=(unsigned)atoi(s+2); assert(mock_length>=256 && mock_length<=259);
        handler(); check_bytes(mock_length); assert(reads==1 && !errors && !events);
    } else if(!strcmp(s,"irq") || !strcmp(s,"wrap")) {
        g_ui32InterruptsSeen=!strcmp(s,"wrap") ? UINT32_MAX : 10;
        g_ui32InterruptsSeenCopy=42; uint32_t expected=g_ui32InterruptsSeen+1;
        HciDrvIntService(); assert(g_ui32InterruptsSeen==expected && g_ui32InterruptsSeenCopy==expected && events==1);
    } else if(!strcmp(s,"empty") || !strcmp(s,"empty-tx")) {
        mock_status=AM_DEVICES_EM9305_NO_DATA_TX;
        if(!strcmp(s,"empty-tx")) enqueue();
        handler(); assert(reads==1 && !errors && !resets && !events && !delivered_length);
        assert(writes == (unsigned)!strcmp(s,"empty-tx"));
    } else if(!strcmp(s,"empty-budget")) {
        mock_status=AM_DEVICES_EM9305_NO_DATA_TX; hold_ready=true; enqueue();
        handler(); assert(reads==HCI_DRV_MAX_HCI_TRANSACTIONS && events==1 && !errors && !writes);
        ready=false; handler(); assert(writes==1 && am_hal_queue_empty(&g_sWriteQueue));
        ready=true; hold_ready=false; mock_status=0; handler(); check_bytes(8); assert(!errors && !resets);
    } else if(!strncmp(s,"budget-",7)) {
        empty_before_success=999; hold_ready=false;
        if(!strcmp(s,"budget-idle")) { drop_after_empty=true; handler(); assert(reads==999 && !events); }
        else if(!strcmp(s,"budget-partial")) {
            accept_limit=4; handler(); assert(reads==1000 && events==1 && delivered_length==4);
            handler(); check_bytes(8); assert(reads==1000);
        } else {
            enqueue(); handler(); assert(reads==1000 && events==1 && !writes);
            check_bytes(8); handler(); assert(writes==1 && am_hal_queue_empty(&g_sWriteQueue));
        }
        assert(!errors && !resets);
    } else if(!strcmp(s,"partial")) {
        mock_length=259; accept_limit=100; handler(); assert(delivered_length==100 && reads==1 && events==1);
        handler(); assert(delivered_length==200 && reads==1 && events==2);
        handler(); check_bytes(259); assert(reads==1 && !g_ui32NumBytes && !g_consumed_bytes && !errors);
    } else if(!strcmp(s,"integrated-full") || !strcmp(s,"integrated-empty")) {
        device_mode=true; chunk_count=2;
        if(!strcmp(s,"integrated-full")) {
            chunks[0]=250; chunks[1]=20; inject_irq=true;
            handler(); check_bytes(270);
            assert(reads==2 && transfers==2 && !errors && !resets);
        } else {
            chunks[0]=0; chunk_count=1;
            handler(); assert(reads==1000 && headers==10000 && events==1 && !errors);
            chunks[0]=20; handler(); check_bytes(20); assert(reads==1001 && !errors);
        }
    } else if(!strncmp(s,"device-",7)) {
        device_mode=true; chunks[0]=0; chunk_count=1;
        if(!strcmp(s,"device-prior-empty")) { chunks[0]=10; chunks[1]=0; chunk_count=2; }
        else if(!strcmp(s,"device-259")) { chunks[0]=255; chunks[1]=4; chunk_count=2; }
        else if(!strncmp(s,"device-full",11) || !strcmp(s,"device-stale-counter")) {
            chunks[0]=250; chunks[1]=20; chunk_count=2;
            inject_irq=strcmp(s,"device-full-no-irq")!=0 && strcmp(s,"device-stale-counter")!=0;
            if(!strcmp(s,"device-stale-counter")) { g_ui32InterruptsSeenCopy=99; HciDrvRadioBoot(false); assert(g_ui32InterruptsSeen==0); }
            if(!strcmp(s,"device-full-wrap")) g_ui32InterruptsSeen=g_ui32InterruptsSeenCopy=UINT32_MAX;
        } else if(!strcmp(s,"device-command-error")) command_error=1;
        else if(!strcmp(s,"device-data-error")) { data_error=1; chunks[0]=20; }
        else if(!strcmp(s,"device-not-ready")) status_byte=0;
        struct { uint32_t before; uint32_t data[65]; uint32_t after; } storage={.before=0x12345678,.after=0x87654321};
        uint32_t n=0; uint32_t status=__real_am_devices_em9305_block_read(&device,storage.data,&n);
        assert(storage.before==0x12345678 && storage.after==0x87654321);
        assert(((uint8_t *)storage.data)[259]==0); assert(bus_enabled==1 && bus_disabled==1);
        if(!strcmp(s,"device-empty")) assert(status==AM_DEVICES_EM9305_NO_DATA_TX && n==0 && headers==10 && transfers==0);
        else if(!strcmp(s,"device-prior-empty")) assert(status==0 && n==10 && transfers==1 && headers==11);
        else if(!strcmp(s,"device-259")) assert(status==0 && n==259 && transfers==2);
        else if(!strncmp(s,"device-full",11) || !strcmp(s,"device-stale-counter")) {
            assert(n==250 && transfers==1 && chunk_index==1);
            assert(status == ((!strcmp(s,"device-full-no-irq") || !strcmp(s,"device-stale-counter")) ? AM_DEVICES_EM9305_DATA_LENGTH_ERROR : 0));
            for(unsigned i=0;i<n;i++) assert(((uint8_t *)storage.data)[i]==(uint8_t)i);
            n=0; status=__real_am_devices_em9305_block_read(&device,storage.data,&n);
            assert(status==0 && n==20 && transfers==2 && !ready);
            for(unsigned i=0;i<n;i++) assert(((uint8_t *)storage.data)[i]==(uint8_t)(250+i));
        } else if(command_error) assert(status==AM_DEVICES_EM9305_CMD_TRANSFER_ERROR && !n && !transfers);
        else if(data_error) assert(status==AM_DEVICES_EM9305_DATA_TRANSFER_ERROR && !n);
        else assert(status==AM_DEVICES_EM9305_NOT_READY && !n && !transfers);
    } else {
        mock_status=!strcmp(s,"not-ready") ? AM_DEVICES_EM9305_NOT_READY :
                    !strcmp(s,"transfer-error") ? AM_DEVICES_EM9305_DATA_TRANSFER_ERROR : AM_DEVICES_EM9305_DATA_LENGTH_ERROR;
        handler(); assert(errors==1 && last_error==mock_status && resets==1 && boots==1 && !delivered_length);
    }
    puts("PASS"); return 0;
}
