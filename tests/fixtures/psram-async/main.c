#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nsx_psram_internal.h"

#define BASE 0x20000000u
#define SIZE (512u * 1024u)
#define PTR(x) ((void *)(uintptr_t)(x))
static uint32_t irq, submissions, calls, delays, completions;
static uint32_t submit_error, blocking_error, complete_error;
static unsigned point, inject_at;
static bool in_callback, delivering, blocked, hold, nested, complete_on_restore;
static nsx_psram_client_t a, b;
static nsx_psram_async_slot_t *queue[NSX_PSRAM_ASYNC_DEPTH];
static unsigned count;
static uint32_t callback_status;

static void complete_one(void) {
    assert(!irq && count);
    nsx_psram_async_slot_t *slot=queue[0];
    memmove(queue, queue+1, (--count)*sizeof(queue[0]));
    in_callback=true;
    ++completions;
    nsx_psram_async_complete(slot, complete_error);
    in_callback=false;
}
uint32_t nsx_psram_platform_irq_save(void) {
    uint32_t old=irq; irq=1; return old;
}
void nsx_psram_platform_irq_restore(uint32_t old) {
    assert(irq); irq=old;
    if (!irq && count && complete_on_restore && !delivering) {
        delivering=true; complete_one(); delivering=false;
    }
}
void nsx_psram_test_point(void) {
    if (irq || in_callback || delivering) return;
    ++point;
    if (count && point==inject_at) {
        delivering=true; complete_one(); delivering=false;
    }
}
void nsx_psram_platform_delay_us(uint32_t us) {
    assert(us==1u); ++delays;
    if (!hold && count && !irq) complete_one();
}
uint32_t nsx_psram_platform_init(const nsx_psram_config_t *cfg,
                                nsx_psram_platform_info_t *info) {
    assert(!nsx_psram_can_sleep());
    info->size_bytes=0x10000u; info->base_address=0x60000000u;
    info->clock_hz=cfg->clock_hz; info->safe_to_retry=true;
    return 0;
}
uint32_t nsx_psram_platform_check_destination(const void *buffer, uint32_t length) {
    assert(irq);
    uintptr_t address=(uintptr_t)buffer;
    if (length>4096 || address<BASE || address-BASE>SIZE-length ||
        (address-BASE)%4096>4096-length) return NSX_PSRAM_STATUS_DESTINATION;
    return 0;
}
static void done(void *ctx, uint32_t status) {
    assert(in_callback && ctx==&calls);
    ++calls; callback_status=status;
}
uint32_t nsx_psram_platform_read_async(uint32_t offset, void *buffer, uint32_t length,
                                     nsx_psram_async_slot_t *slot, bool previous) {
    (void)offset; (void)buffer; (void)length;
    assert(irq && slot->queued && slot->client && slot->done==done);
    assert(slot->client->issued-slot->client->completed>0u);
    assert(previous==(count>0));
    ++submissions;
    if (submit_error) return submit_error;
    assert(count<NSX_PSRAM_ASYNC_DEPTH); queue[count++]=slot;
    return 0;
}
static uint32_t submit(nsx_psram_client_t *client) {
    return nsx_psram_read_async(client, 0, PTR(BASE), 4, done, &calls);
}
static uint32_t blocking(void) {
    assert(!nsx_psram_can_sleep());
    if (nested) {
        nested=false;
        assert(submit(&a)==NSX_PSRAM_STATUS_BUSY);
        assert(nsx_psram_read(0, PTR(BASE), 4)==NSX_PSRAM_STATUS_BUSY);
        assert(nsx_psram_write(0, PTR(BASE), 4)==NSX_PSRAM_STATUS_BUSY);
    }
    blocked=true; return blocking_error;
}
uint32_t nsx_psram_platform_read(uint32_t o, void *bfr, uint32_t n) {
    (void)o; (void)bfr; (void)n; return blocking();
}
uint32_t nsx_psram_platform_write(uint32_t o, const void *bfr, uint32_t n) {
    (void)o; (void)bfr; (void)n; return blocking();
}
static void all_done(void) { while(count) complete_one(); }
static void snapshot(void) {
    unsigned old=irq;
    uint32_t n=nsx_psram_pending();
    /* A completion can be injected after snapshot; the returned value is still
     * a valid earlier snapshot, never a wrapped negative or mixed read. */
    assert(n>=count && n<=NSX_PSRAM_ASYNC_DEPTH);
    assert(irq==old);
}
int main(int argc, char **argv) {
    assert(argc>=2);
    nsx_psram_config_t cfg;
    nsx_psram_info_t info;
    assert(!nsx_psram_can_sleep());
    assert(nsx_psram_client_pending(NULL)==UINT32_MAX);
    assert(nsx_psram_client_drain(NULL, 1)==NSX_STATUS_INVALID_HANDLE);
    assert(submit(NULL)==NSX_STATUS_INVALID_HANDLE);
    assert(submit(&a)==NSX_PSRAM_STATUS_NOT_INITIALIZED);
    assert(nsx_psram_default_config(&cfg)==0);
    assert(cfg.api->version.minor==1 && nsx_psram_V1_0_0.version.minor==0);
    assert(nsx_psram_V1_1_0.version.minor==1);
    assert(nsx_psram_oldest_supported_version.version.minor==0);
    cfg.api=&nsx_psram_V1_0_0; assert(nsx_psram_init(&cfg)==0);
    assert(nsx_psram_get_info(&info)==0);
#ifdef WITHOUT_ASYNC
    (void)b;
    assert(info.async_depth==0 && !(info.capabilities&NSX_PSRAM_CAP_ASYNC_READ));
    assert(submit(&a)==NSX_PSRAM_STATUS_UNSUPPORTED);
    assert(nsx_psram_read_async(&a, 0, NULL, 0, done, &calls)==0);
    assert(!submissions && !count && !calls);
    return 0;
#else
    assert(info.async_depth==NSX_PSRAM_ASYNC_DEPTH && info.capabilities==15u);
    assert(!info.last_transfer_status && nsx_psram_can_sleep());
    if (!strcmp(argv[1], "checks")) {
        assert(nsx_psram_read_async(&a, 0, PTR(BASE), 4, NULL, &calls)==NSX_STATUS_INVALID_HANDLE);
        assert(nsx_psram_read_async(&a, UINT32_MAX, NULL, 0, done, &calls)==0);
        assert(nsx_psram_read_async(&a, 0, NULL, 4, done, &calls)==NSX_STATUS_INVALID_CONFIG);
        assert(nsx_psram_read_async(&a, 0xffffu, PTR(BASE), 4, done, &calls)==NSX_PSRAM_STATUS_OUT_OF_RANGE);
        assert(nsx_psram_read_async(&a, UINT32_MAX, PTR(BASE), 4, done, &calls)==NSX_PSRAM_STATUS_OUT_OF_RANGE);
        assert(nsx_psram_read_async(&a, 1, PTR(BASE), 4, done, &calls)==NSX_PSRAM_STATUS_ALIGNMENT);
        assert(nsx_psram_read_async(&a, 0, PTR(BASE+1), 4, done, &calls)==NSX_PSRAM_STATUS_ALIGNMENT);
        assert(nsx_psram_read_async(&a, 0, PTR(BASE), 3, done, &calls)==NSX_PSRAM_STATUS_ALIGNMENT);
        const uintptr_t invalid[]={BASE-4,BASE+4092,BASE+SIZE-4,BASE+SIZE,0x30000000u,UINTPTR_MAX-3};
        for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
            assert(nsx_psram_read_async(&a, 0, PTR(invalid[i]), 8, done, &calls)==NSX_PSRAM_STATUS_DESTINATION);
        assert(nsx_psram_read_async(&a, 0, PTR(BASE), 4100, done, &calls)==NSX_PSRAM_STATUS_DESTINATION);
        assert(nsx_psram_read(1,PTR(BASE),4)==NSX_PSRAM_STATUS_ALIGNMENT);
        assert(nsx_psram_write(0,PTR(BASE),3)==NSX_PSRAM_STATUS_ALIGNMENT);
        assert(!submissions && !calls && !a.issued && !a.completed && !blocked);
    } else if (!strcmp(argv[1], "depth")) {
        irq=1;
        for(unsigned i=0;i<NSX_PSRAM_ASYNC_DEPTH;++i) assert(submit(&a)==0);
        assert(submit(&a)==NSX_PSRAM_STATUS_BUSY);
        assert(nsx_psram_pending()==NSX_PSRAM_ASYNC_DEPTH && irq==1);
        irq=0; complete_one(); assert(submit(&a)==0); all_done();
        assert(calls==NSX_PSRAM_ASYNC_DEPTH+1 && nsx_psram_can_sleep());
    } else if (!strcmp(argv[1], "clients")) {
        assert(submit(&a)==0);
        hold=true;
        assert(nsx_psram_client_drain(&a,0)==NSX_PSRAM_STATUS_TIMEOUT);
        assert(nsx_psram_client_drain(&a,3)==NSX_PSRAM_STATUS_TIMEOUT && delays==3);
        complete_one(); assert(submit(&b)==0);
        assert(nsx_psram_client_drain(&a,0)==0);
        assert(nsx_psram_client_pending(&b)==1 && nsx_psram_pending()==1);
        assert(!nsx_psram_can_sleep());
        hold=false; assert(nsx_psram_client_drain(&b,10)==0);
        assert(calls==2 && !callback_status && nsx_psram_can_sleep());
    } else if (!strcmp(argv[1], "errors")) {
        submit_error=77; assert(submit(&a)==77);
        assert(!a.issued && !a.completed && !nsx_psram_pending() && !calls);
        submit_error=0; assert(submit(&a)==0);
        if (NSX_PSRAM_ASYNC_DEPTH>1) {
            submit_error=88; assert(submit(&b)==88); assert(!b.issued);
            submit_error=0;
        }
        complete_error=99; complete_one();
        assert(calls==1 && callback_status==NSX_PSRAM_STATUS_TRANSFER_FAILED);
        assert(nsx_psram_get_info(&info)==0 && info.last_transfer_status==99);
        assert(info.state==NSX_PSRAM_STATE_READY);
        assert(nsx_psram_client_drain(&a,0)==0);
        complete_error=0; assert(submit(&b)==0); complete_one();
        assert(nsx_psram_get_info(&info)==0 && info.last_transfer_status==99);
    } else if (!strcmp(argv[1], "blocking") || !strcmp(argv[1], "blocking-error")) {
        assert(submit(&a)==0);
        assert(nsx_psram_read(0,PTR(BASE),4)==NSX_PSRAM_STATUS_BUSY);
        assert(nsx_psram_write(0,PTR(BASE),4)==NSX_PSRAM_STATUS_BUSY);
        assert(!blocked); all_done(); nested=true;
        assert(nsx_psram_read(0,PTR(BASE),4)==0);
        assert(nsx_psram_can_sleep());
        if (!strcmp(argv[1], "blocking-error")) {
            blocking_error=77;
            assert(nsx_psram_write(0,PTR(BASE),4)==77);
            assert(!nsx_psram_can_sleep());
            assert(submit(&a)==NSX_PSRAM_STATUS_FAILED_STATE);
            assert(nsx_psram_read(0,PTR(BASE),4)==NSX_PSRAM_STATUS_FAILED_STATE);
            assert(nsx_psram_get_info(&info)==0 && info.state==NSX_PSRAM_STATE_FAILED);
        } else { assert(submit(&a)==0); all_done(); }
    } else if (!strcmp(argv[1], "wrap")) {
        a.issued=UINT32_MAX; a.completed=UINT32_MAX;
        g_async_issued=UINT32_MAX; g_async_completed=UINT32_MAX;
        assert(submit(&a)==0 && a.issued==0 && a.completed==UINT32_MAX);
        assert(nsx_psram_client_pending(&a)==1 && nsx_psram_pending()==1);
        complete_one(); assert(a.completed==0 && nsx_psram_client_pending(&a)==0);
        assert(submit(&a)==0); complete_one();
    } else if (!strcmp(argv[1], "early-completion")) {
        complete_on_restore=true;
        assert(submit(&a)==0);
        assert(calls==1 && !count && !nsx_psram_pending());
    } else if (!strcmp(argv[1], "interleave")) {
        assert(argc==3); inject_at=(unsigned)atoi(argv[2]);
        assert(submit(&a)==0); snapshot();
        uint32_t second=submit(&b);
        assert(second==0 || (NSX_PSRAM_ASYNC_DEPTH==1 && second==NSX_PSRAM_STATUS_BUSY));
        snapshot(); submit_error=77;
        uint32_t refused=submit(&a);
        assert(refused==77 || refused==NSX_PSRAM_STATUS_BUSY);
        snapshot(); submit_error=0;
        assert(nsx_psram_client_drain(&a,10)==0);
        all_done(); snapshot();
        assert(a.issued==a.completed && b.issued==b.completed);
        assert(calls==completions && calls==(second==0?2u:1u));
        irq=1;
        for(unsigned i=0;i<NSX_PSRAM_ASYNC_DEPTH;++i) assert(submit(&a)==0);
        assert(submit(&b)==NSX_PSRAM_STATUS_BUSY);
        irq=0;
        all_done(); assert(!nsx_psram_pending());
        printf("%u\n",point);
    } else { assert(0); }
    assert(!irq);
    return 0;
#endif
}
