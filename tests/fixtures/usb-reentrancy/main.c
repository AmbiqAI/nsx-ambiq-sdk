#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nsx_core.h"
#include "nsx_timer.h"
#include "nsx_usb.h"
#include "tusb.h"
const int nsx_timer_V1_0_0=0;
const uint8_t desc_ms_os_20[10]={0};
const uint8_t *nsx_usb_get_webusb_url_desc(uint8_t *len) {*len=0;return NULL;}
static nsx_timer_config_t *timer;
static unsigned task_depth,max_depth,task_calls,callback_depth,max_callback_depth;
static unsigned callbacks,received,available,emit,inject,write_count;
static bool vendor, snapshot;
uint32_t nsx_timer_init(nsx_timer_config_t *cfg) {timer=cfg;return 0;}
bool tusb_init(void) {return true;}
void am_util_delay_ms(uint32_t ms) {(void)ms;}
static void preempt(void) {
    if (inject) {
        inject=0;
        unsigned before=task_calls;
        timer->callback(timer);
        assert(task_calls==before+(snapshot?1:0)); /* Snapshots allow timer progress. */
    }
}
static void receive_callback(nsx_usb_config_t *cfg) {
    ++callbacks;
    ++callback_depth;
    if(callback_depth>max_callback_depth)max_callback_depth=callback_depth;
    uint8_t bytes[8]; uint32_t n=0;
    if(vendor) assert(nsx_usb_vendor_read_nb(cfg,bytes,sizeof(bytes),&n)==0);
    else assert(nsx_usb_read_nb(cfg,bytes,sizeof(bytes),&n)==0);
    for(unsigned i=0;i<n;++i)assert(bytes[i]==0x5a);
    received+=n;
    unsigned before=task_calls;
    timer->callback(timer); /* Preemption after inner read must still be excluded. */
    assert(task_calls==before);
    --callback_depth;
}
void tud_task(void) {
    ++task_calls;
    ++task_depth;
    if(task_depth>max_depth)max_depth=task_depth;
    if(emit) {
        emit=0;
        if(vendor)tud_vendor_rx_cb(0,NULL,0);else tud_cdc_rx_cb(0);
    }
    --task_depth;
}
uint32_t tud_cdc_available(void) {preempt();return vendor?0:available;}
uint32_t tud_vendor_available(void) {preempt();return vendor?available:0;}
uint32_t tud_cdc_write_available(void) {preempt();return 1024;}
uint32_t tud_vendor_write_available(void) {preempt();return 1024;}
uint32_t tud_cdc_write(const void *p,uint32_t n) {(void)p;write_count+=n;preempt();return n;}
uint32_t tud_vendor_write(const void *p,uint32_t n) {return tud_cdc_write(p,n);}
uint32_t tud_cdc_write_flush(void) {preempt();return 0;}
uint32_t tud_vendor_flush(void) {preempt();return 0;}
static uint32_t read_bytes(void *p,uint32_t n) {preempt();if(n>available)n=available;memset(p,0x5a,n);available-=n;return n;}
uint32_t tud_cdc_read(void *p,uint32_t n) {return read_bytes(p,n);}
uint32_t tud_vendor_read(void *p,uint32_t n) {return read_bytes(p,n);}
void tud_cdc_read_flush(void) {preempt();available=0;}
bool tud_cdc_connected(void) {preempt();return true;}
bool tud_control_xfer(uint8_t p,const tusb_control_request_t *r,void *d,uint16_t n) {(void)p;(void)r;(void)d;(void)n;return true;}
bool tud_control_status(uint8_t p,const tusb_control_request_t *r) {(void)p;(void)r;return true;}
int main(int argc,char **argv) {
    assert(argc==3);
    vendor=!strcmp(argv[1],"vendor");
    uint8_t rx[1024],tx[1024],bytes[8]={0}; uint32_t n;
    nsx_usb_config_t cfg={.rx_buffer=rx,.tx_buffer=tx,.rx_buffer_len=sizeof(rx),.tx_buffer_len=sizeof(tx),.timeout_ms=2};
    cfg.rx_cb=receive_callback;cfg.vendor_rx_cb=receive_callback;
    assert(nsx_usb_init(&cfg)==0);cfg._vendor_connected=1;
    available=8;
    if(!strcmp(argv[2],"query")) {
        snapshot=true;
        for(unsigned i=0;i<100;++i) {
            cfg._rx_ready=cfg._vendor_rx_ready=0;
            available=0;inject=1;
            if(vendor)assert(!nsx_usb_vendor_data_available(&cfg));else assert(!nsx_usb_data_available(&cfg));
            assert(!inject);
        }
    } else if(!strcmp(argv[2],"flush")) {
        inject=1;nsx_usb_flush_rx(&cfg);assert(!inject && available==0);
    } else if(!strcmp(argv[2],"connected")) {
        snapshot=true;available=0;
        for(unsigned i=0;i<100;++i) {
            inject=1;assert(nsx_usb_connected(&cfg));assert(!inject);
        }
    } else {
        emit=1;
        if(!strcmp(argv[2],"timer"))timer->callback(timer);
        else if(!strcmp(argv[2],"fallback")) {emit=0;timer->callback(timer);}
        else if(!strcmp(argv[2],"send")) {
            if(vendor)assert(nsx_usb_vendor_send(&cfg,bytes,8,&n)==0);
            else assert(nsx_usb_send(&cfg,bytes,8,&n)==0);
            assert(n==8 && write_count==8);
        } else {
            if(vendor)assert(nsx_usb_vendor_write_available(&cfg)==1024);
            else assert(nsx_usb_read_nb(&cfg,bytes,8,&n)==0);
        }
        assert(callbacks==1 && received==8 && max_callback_depth==1 && max_depth==1);
    }
    unsigned before=task_calls;
    available=0;timer->callback(timer);
    assert(task_calls==before+1); /* No guard leakage. */
    puts("PASS");return 0;
}
