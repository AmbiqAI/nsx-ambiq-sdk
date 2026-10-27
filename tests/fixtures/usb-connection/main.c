#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nsx_core.h"
#include "nsx_timer.h"
#include "nsx_usb.h"
#include "tusb.h"

const int nsx_timer_V1_0_0 = 0;
const uint8_t desc_ms_os_20[10] = {0};
const uint8_t *nsx_usb_get_webusb_url_desc(uint8_t *len) {*len=0;return NULL;}
void tud_mount_cb(void);
void tud_umount_cb(void);
void tud_suspend_cb(bool);
void tud_resume_cb(void);
bool tud_vendor_control_xfer_cb(uint8_t, uint8_t, const tusb_control_request_t *);
static bool ready, dtr;
static unsigned serviced, delayed, teardown, tx_space=16, rx_count, written;
static uint8_t wire[32];
static enum {NONE, UNPLUG, SUSPEND, OPEN} pending;
#ifdef TUSB_ADDED_FUNCTIONS
/* Model the DCD owning unmount without touching NSX's session flag. */
void tud_umount_cb(void) {++teardown;}
#endif
static void vendor_open(bool open) {
    tusb_control_request_t request={.bmRequestType_bit={.type=TUSB_REQ_TYPE_CLASS},
        .bRequest=0x22,.wValue=open ? 1 : 0};
    assert(tud_vendor_control_xfer_cb(0,CONTROL_STAGE_SETUP,&request));
}
uint32_t nsx_timer_init(nsx_timer_config_t *cfg) {(void)cfg;return 0;}
bool tusb_init(void) {return true;}
bool tud_ready(void) {return ready;}
bool tud_cdc_connected(void) {return ready && dtr;}
void am_util_delay_ms(uint32_t ms) {delayed+=ms;}
void tud_task(void) {
    ++serviced;
    switch(pending) {
    case UNPLUG:ready=false;tud_umount_cb();break;
    case SUSPEND:ready=false;tud_suspend_cb(false);break;
    case OPEN:vendor_open(true);break;
    default:break;
    }
    pending=NONE;
}
uint32_t tud_cdc_write_available(void) {return tx_space;}
uint32_t tud_vendor_write_available(void) {return tx_space;}
uint32_t tud_cdc_write(const void *data,uint32_t len) {
    unsigned n=len<tx_space?len:tx_space;
    assert(written+n<=sizeof(wire));memcpy(wire+written,data,n);
    written+=n;tx_space-=n;return n;
}
uint32_t tud_vendor_write(const void *data,uint32_t len) {return tud_cdc_write(data,len);}
uint32_t tud_cdc_write_flush(void) {return 0;}
uint32_t tud_vendor_flush(void) {return 0;}
uint32_t tud_cdc_available(void) {return rx_count;}
uint32_t tud_vendor_available(void) {return rx_count;}
uint32_t tud_cdc_read(void *data,uint32_t len) {
    unsigned n=len<rx_count?len:rx_count;memset(data,0x5a,n);rx_count-=n;return n;
}
uint32_t tud_vendor_read(void *data,uint32_t len) {return tud_cdc_read(data,len);}
void tud_cdc_read_flush(void) {rx_count=0;}
bool tud_control_xfer(uint8_t p,const tusb_control_request_t *r,void *data,uint16_t n) {
    (void)p;(void)r;(void)data;(void)n;return true;
}
bool tud_control_status(uint8_t p,const tusb_control_request_t *r) {(void)p;(void)r;return true;}

int main(int argc,char **argv) {
    assert(argc==2);
    uint8_t rx[1024],tx[1024],data[24],received[8];memset(data,0xa5,sizeof(data));
    nsx_usb_config_t cfg={.rx_buffer=rx,.tx_buffer=tx,.rx_buffer_len=sizeof(rx),
        .tx_buffer_len=sizeof(tx),.timeout_ms=2};
    uint32_t count=99;
    if(!strcmp(argv[1],"invalid")) {
        ready=dtr=true;cfg._vendor_connected=1;
        assert(!nsx_usb_connected(NULL) && !nsx_usb_connected(&cfg));
        assert(!nsx_usb_vendor_connected(NULL) && !nsx_usb_vendor_connected(&cfg));
        assert(nsx_usb_vendor_write_available(NULL)==0 && nsx_usb_vendor_write_available(&cfg)==0);
        assert(serviced==0);puts("PASS");return 0;
    }
    assert(nsx_usb_init(&cfg)==0);
    if(!strcmp(argv[1],"cdc-dtr")) {
        for(unsigned r=0;r<2;++r)for(unsigned d=0;d<2;++d) {
            ready=r;dtr=d;assert(nsx_usb_connected(&cfg)==(ready&&dtr));
        }
        assert(serviced==0);
    } else if(!strcmp(argv[1],"vendor-lifecycle")) {
        ready=true;assert(!nsx_usb_vendor_connected(&cfg));
        vendor_open(true);assert(nsx_usb_vendor_connected(&cfg));
        ready=false;assert(!nsx_usb_vendor_connected(&cfg));
        tud_umount_cb();
#ifdef TUSB_ADDED_FUNCTIONS
        assert(teardown==1 && cfg._vendor_connected==1);
#else
        assert(teardown==0 && cfg._vendor_connected==0);
#endif
        ready=true;tud_mount_cb();assert(!nsx_usb_vendor_connected(&cfg));
        vendor_open(true);assert(nsx_usb_vendor_connected(&cfg));
        ready=false;tud_suspend_cb(false);assert(!nsx_usb_vendor_connected(&cfg));
        ready=true;tud_resume_cb();assert(!nsx_usb_vendor_connected(&cfg));
        vendor_open(true);assert(nsx_usb_vendor_connected(&cfg));
        vendor_open(false);assert(!nsx_usb_vendor_connected(&cfg));
        assert(serviced==0);
    } else if(!strcmp(argv[1],"queued-unplug") || !strcmp(argv[1],"queued-suspend")) {
        ready=true;vendor_open(true);
        pending=!strcmp(argv[1],"queued-unplug")?UNPLUG:SUSPEND;
        assert(nsx_usb_vendor_write_available(&cfg)==0);
        assert(serviced==1 && !nsx_usb_vendor_connected(&cfg));
    } else if(!strcmp(argv[1],"queued-open")) {
        ready=true;pending=OPEN;
        assert(nsx_usb_vendor_write_available(&cfg)==tx_space);
        assert(serviced==1 && nsx_usb_vendor_connected(&cfg));
    } else if(!strcmp(argv[1],"cdc-buffers") || !strcmp(argv[1],"vendor-buffers")) {
        bool vendor=!strcmp(argv[1],"vendor-buffers");
        ready=true; /* Enumerated; CDC DTR and vendor logical session are false. */
        assert(!nsx_usb_connected(&cfg) && !nsx_usb_vendor_connected(&cfg));
        uint32_t status=vendor?nsx_usb_vendor_send(&cfg,data,24,&count):nsx_usb_send(&cfg,data,24,&count);
        assert(status==NSX_USB_STATUS_TIMEOUT && count==16 && delayed==2);
        assert(!memcmp(wire,data,16));
        ready=false;rx_count=4; /* Buffered reads remain usable during suspend. */
        status=vendor?nsx_usb_vendor_read_nb(&cfg,received,8,&count):nsx_usb_read_nb(&cfg,received,8,&count);
        assert(status==0 && count==4 && received[0]==0x5a && received[3]==0x5a);
        status=vendor?nsx_usb_vendor_read_nb(&cfg,received,8,&count):nsx_usb_read_nb(&cfg,received,8,&count);
        assert(status==0 && count==0);
    } else if(!strcmp(argv[1],"receive-status")) {
        rx_count=4;assert(nsx_usb_receive(&cfg,received,8,&count)==NSX_USB_STATUS_PARTIAL && count==4);
        assert(nsx_usb_receive(&cfg,received,8,&count)==NSX_USB_STATUS_TIMEOUT && count==0);
        rx_count=8;assert(nsx_usb_receive(&cfg,received,8,&count)==0 && count==8);
    } else if(!strcmp(argv[1],"reconnect")) {
        for(unsigned i=0;i<2;++i) {
            ready=dtr=true;tud_mount_cb();vendor_open(true);
            assert(nsx_usb_connected(&cfg) && nsx_usb_vendor_connected(&cfg));
            tx_space=8;assert(nsx_usb_send(&cfg,data,4,&count)==0 && count==4);
            assert(nsx_usb_vendor_send(&cfg,data,4,&count)==0 && count==4);
            ready=dtr=false;tud_umount_cb();tx_space=0;
            assert(!nsx_usb_connected(&cfg) && !nsx_usb_vendor_connected(&cfg));
            assert(nsx_usb_send(&cfg,data,4,&count)==NSX_USB_STATUS_TIMEOUT && count==0);
            assert(nsx_usb_vendor_send(&cfg,data,4,&count)==NSX_USB_STATUS_TIMEOUT && count==0);
        }
    } else assert(false);
    puts("PASS");return 0;
}
