#include "am_mcu_apollo.h"
#include "nsx_system.h"
#include "nsx_psram.h"
#include "unity.h"
#include "result_reporter.h"
#include <string.h>

#define WORDS (4096u / sizeof(uint32_t))
#define SOURCE_OFFSET 0x20000u
#define XIP_OFFSET (SOURCE_OFFSET+4096u)
static uint32_t source[WORDS], xip_source[WORDS];
static uint32_t buffers[(NSX_TEST_ASYNC_DEPTH+2)*WORDS] __attribute__((aligned(4096)));
#define SLOT(i) (buffers+(i)*WORDS)
static nsx_psram_info_t info;
static bool ready;
static volatile uint32_t callback_sequence;
typedef struct { volatile uint32_t count, status, ipsr, order; } completion_t;
static completion_t records[NSX_TEST_ASYNC_DEPTH+1];

#ifdef NSX_TEST_DONE_EARLY
uint32_t __real_nsx_psram_read_async(nsx_psram_client_t *,uint32_t,void *,uint32_t,nsx_psram_done_fn,void *);
uint32_t __wrap_nsx_psram_read_async(nsx_psram_client_t *c,uint32_t o,void *b,uint32_t n,nsx_psram_done_fn f,void *x) {
    f(x,0);
    return __real_nsx_psram_read_async(c,o,b,n,f,x);
}
#endif
#if defined(NSX_TEST_GLOBAL_DRAIN) || defined(NSX_TEST_DRAIN_EARLY)
uint32_t __wrap_nsx_psram_client_drain(const nsx_psram_client_t *c,uint32_t timeout) {
    (void)c;
#ifdef NSX_TEST_GLOBAL_DRAIN
    while (nsx_psram_pending()) {
        if (!timeout) return NSX_PSRAM_STATUS_TIMEOUT;
        am_hal_delay_us(1); --timeout;
    }
#else
    (void)timeout;
#endif
    return 0;
}
#endif
void setUp(void) {}
void tearDown(void) { __enable_irq(); }
static void done(void *ctx,uint32_t status) {
    completion_t *record=ctx;
    record->status=status;
    record->ipsr=__get_IPSR();
    record->order=++callback_sequence;
    __DMB();
    ++record->count;
}
static void prepare(void) {
    TEST_ASSERT_TRUE(ready);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_pending());
    memset(buffers,0x5a,sizeof(buffers));
    memset(records,0,sizeof(records));
    callback_sequence=0;
}
static void check_record(unsigned i) {
    TEST_ASSERT_EQUAL_UINT32(1,records[i].count);
    TEST_ASSERT_EQUAL_UINT32(0,records[i].status);
    TEST_ASSERT_TRUE(records[i].ipsr>=16u);
}
static void initialize(void) {
    nsx_system_config_t system={.enable_cache=true,.perf_mode=NSX_TEST_CPU_HIGH?NSX_PERF_HIGH:NSX_PERF_LOW,
                                .debug={.transport=NSX_DEBUG_ITM}};
    TEST_ASSERT_EQUAL_UINT32(0,nsx_system_init(&system));
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_CPU_HIGH?250000000u:96000000u,SystemCoreClock);
    __enable_irq();
    nsx_psram_config_t config;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_default_config(&config));
    config.clock_hz=NSX_TEST_PSRAM_CLOCK;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_init(&config));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_get_info(&info));
    TEST_ASSERT_TRUE(info.capabilities & NSX_PSRAM_CAP_ASYNC_READ);
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_ASYNC_DEPTH,info.async_depth);
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_PSRAM_CLOCK,info.configured_clock_hz);
    TEST_ASSERT_TRUE((uintptr_t)buffers>=DTCM_BASEADDR);
    TEST_ASSERT_TRUE((uintptr_t)buffers+sizeof(buffers)<=DTCM_BASEADDR+DTCM_MAX_SIZE);
    for(unsigned i=0;i<WORDS;++i) {
        source[i]=0x713a9c55u^(i*2654435761u);
        xip_source[i]=~source[i];
    }
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_write(SOURCE_OFFSET,source,sizeof(source)));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_write(XIP_OFFSET,xip_source,sizeof(xip_source)));
    ready=true;
}
static void dtcm_sizes_and_edges(void) {
    for(unsigned size=4;size<=4096;size+=4) {
        for(unsigned edge=0;edge<2;++edge) {
            prepare();
            static nsx_psram_client_t client; client=(nsx_psram_client_t){0};
            unsigned words=size/4;
            uint32_t *destination=SLOT(1)+(edge?WORDS-words:0);
            TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_read_async(&client,SOURCE_OFFSET,destination,size,done,&records[0]));
            TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
            TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_pending(&client));
            check_record(0);
            TEST_ASSERT_EQUAL_MEMORY(source,destination,size);
            TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a, destination[-1]);
            TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a, destination[words]);
        }
    }
}
static void mixed_lengths(void) {
    prepare(); static nsx_psram_client_t client; client=(nsx_psram_client_t){0};
    const unsigned sizes[]={4096,4,68};
    uint32_t statuses[NSX_TEST_ASYNC_DEPTH];
    __disable_irq();
    for(unsigned i=0;i<NSX_TEST_ASYNC_DEPTH;++i)
        statuses[i]=nsx_psram_read_async(&client,SOURCE_OFFSET,SLOT(i+1),sizes[i%3],done,&records[i]);
    uint32_t pending=nsx_psram_client_pending(&client);
    uint32_t callbacks=callback_sequence;
    __enable_irq();
    for(unsigned i=0;i<NSX_TEST_ASYNC_DEPTH;++i) TEST_ASSERT_EQUAL_UINT32(0,statuses[i]);
    TEST_ASSERT_EQUAL_UINT32(NSX_TEST_ASYNC_DEPTH,pending);
    TEST_ASSERT_EQUAL_UINT32(0,callbacks);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
    for(unsigned i=0;i<NSX_TEST_ASYNC_DEPTH;++i) {
        check_record(i);
        TEST_ASSERT_EQUAL_MEMORY(source,SLOT(i+1),sizes[i%3]);
        for(unsigned word=sizes[i%3]/4;word<WORDS;++word)
            TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a,SLOT(i+1)[word]);
    }
    TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a,SLOT(1)[-1]);
    TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a,SLOT(NSX_TEST_ASYNC_DEPTH+1)[0]);
}
static void queue_depth(void) {
    prepare(); static nsx_psram_client_t client; client=(nsx_psram_client_t){0};
    uint32_t status[NSX_TEST_ASYNC_DEPTH+1];
    __disable_irq();
    for(unsigned i=0;i<info.async_depth;++i)
        status[i]=nsx_psram_read_async(&client,SOURCE_OFFSET,SLOT(i+1),4096,done,&records[i]);
    status[info.async_depth]=nsx_psram_read_async(&client,SOURCE_OFFSET,SLOT(1),4096,done,&records[info.async_depth]);
    uint32_t pending=nsx_psram_pending();
    __enable_irq();
    for(unsigned i=0;i<info.async_depth;++i) TEST_ASSERT_EQUAL_UINT32(0,status[i]);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_BUSY,status[info.async_depth]);
    TEST_ASSERT_EQUAL_UINT32(info.async_depth,pending);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
    for(unsigned i=0;i<info.async_depth;++i) {
        check_record(i);
        TEST_ASSERT_EQUAL_UINT32(i+1,records[i].order);
        TEST_ASSERT_EQUAL_MEMORY(source,SLOT(i+1),4096);
    }
    TEST_ASSERT_EQUAL_UINT32(0,records[info.async_depth].count);
}
static void xip_during_dma(void) {
    prepare();
    unsigned overlap=0,starts=0,ends=0;
    const volatile uint32_t *xip=(const volatile uint32_t *)(uintptr_t)(info.base_address+XIP_OFFSET);
    /* Each pass gets a fresh bounded burst, so overlap is observable without
     * requiring a slow CPU loop to beat completion of one fixed initial queue. */
    for(unsigned pass=0;pass<64;++pass) {
        static nsx_psram_client_t client; client=(nsx_psram_client_t){0};
        am_hal_cachectrl_range_t range={.ui32StartAddr=(uint32_t)(uintptr_t)xip,.ui32Size=64};
        TEST_ASSERT_EQUAL_UINT32(0,am_hal_cachectrl_dcache_invalidate(&range,false));
        __disable_irq();
        uint32_t status=0;
        for(unsigned i=0;i<info.async_depth && status==0;++i)
            status=nsx_psram_read_async(&client,SOURCE_OFFSET,SLOT(i+1),4096,done,&records[i]);
        bool before=false,after=false;
        uint32_t start_status=nsx_psram_dma_in_progress(&before);
        /* CQ startup is asynchronous too: wait a bounded number of samples for
         * actual DMA activity before starting the measured XIP pass. */
        for(unsigned sample=0;sample<10000 && start_status==0 && !before;++sample)
            start_status=nsx_psram_dma_in_progress(&before);
        unsigned bad=0;
        for(unsigned i=0;i<16;++i) if(xip[i]!=xip_source[i]) ++bad;
        uint32_t end_status=nsx_psram_dma_in_progress(&after);
        __enable_irq();
        TEST_ASSERT_EQUAL_UINT32(0,status);
        TEST_ASSERT_EQUAL_UINT32(0,start_status);
        TEST_ASSERT_EQUAL_UINT32(0,end_status);
        TEST_ASSERT_EQUAL_UINT32(0,bad);
        if(before) ++starts;
        if(after) ++ends;
        if(before && after) ++overlap;
        TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
        for(unsigned i=0;i<info.async_depth;++i) {
            TEST_ASSERT_EQUAL_UINT32(pass+1,records[i].count);
            TEST_ASSERT_EQUAL_UINT32(0,records[i].status);
            TEST_ASSERT_TRUE(records[i].ipsr>=16);
            TEST_ASSERT_EQUAL_MEMORY(source,SLOT(i+1),4096);
        }
    }
    UnityPrint("DMA/XIP qualifying passes=");UnityPrintNumberUnsigned(overlap);UnityPrint("\n");
    UnityPrint("TIP start/end=");UnityPrintNumberUnsigned(starts);UnityPrint("/");UnityPrintNumberUnsigned(ends);UnityPrint("\n");
    TEST_ASSERT_TRUE(overlap>0);
}
static void two_clients(void) {
    prepare(); static nsx_psram_client_t a,b; a=(nsx_psram_client_t){0}; b=(nsx_psram_client_t){0};
    __disable_irq();
    uint32_t submit=nsx_psram_read_async(&a,SOURCE_OFFSET,SLOT(1),4096,done,&records[0]);
    uint32_t drain=nsx_psram_client_drain(&a,0);
    uint32_t still_pending=nsx_psram_client_pending(&a);
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(0,submit);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_TIMEOUT,drain);
    TEST_ASSERT_EQUAL_UINT32(1,still_pending);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&a,100000));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_pending(&a));
    check_record(0);TEST_ASSERT_EQUAL_MEMORY(source,SLOT(1),4096);
    /* Stage B after A completes; a single HAL ISR can dispatch multiple CQ
     * callbacks, so masking its IRQ from A's callback cannot hold B reliably. */
    __disable_irq();
    submit=nsx_psram_read_async(&b,SOURCE_OFFSET,SLOT(2),4096,done,&records[1]);
    drain=nsx_psram_client_drain(&a,0);
    still_pending=nsx_psram_client_pending(&b);
    bool sleep=nsx_psram_can_sleep();
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(0,submit);
    TEST_ASSERT_EQUAL_UINT32(0,drain);
    TEST_ASSERT_EQUAL_UINT32(1,still_pending);TEST_ASSERT_FALSE(sleep);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&b,100000));
    check_record(1);TEST_ASSERT_EQUAL_MEMORY(source,SLOT(2),4096);
}
static void refusals(void) {
    prepare(); static nsx_psram_client_t c; c=(nsx_psram_client_t){0};
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,nsx_psram_read_async(&c,1,SLOT(1),4,done,&records[0]));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,nsx_psram_read_async(&c,0,SLOT(1),3,done,&records[0]));
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_ALIGNMENT,nsx_psram_read_async(&c,0,(uint8_t *)SLOT(1)+1,4,done,&records[0]));
    uintptr_t invalid[]={ (uintptr_t)SLOT(1)+4092,0x30000000u,0x20080000u,
                          DTCM_BASEADDR+DTCM_MAX_SIZE-4u,0x20100000u,0x20180000u };
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
        TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_DESTINATION,nsx_psram_read_async(&c,0,(void *)invalid[i],8,done,&records[0]));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_pending());
    TEST_ASSERT_EQUAL_UINT32(0,c.issued);TEST_ASSERT_EQUAL_UINT32(0,records[0].count);
}
static void blocking_exclusion(void) {
    prepare();static nsx_psram_client_t c; c=(nsx_psram_client_t){0};
    __disable_irq();
    uint32_t submit=nsx_psram_read_async(&c,SOURCE_OFFSET,SLOT(1),4096,done,&records[0]);
    uint32_t read=nsx_psram_read(SOURCE_OFFSET,SLOT(2),4);
    uint32_t write=nsx_psram_write(SOURCE_OFFSET,source,4);
    bool sleep=nsx_psram_can_sleep();
    __enable_irq();
    TEST_ASSERT_EQUAL_UINT32(0,submit);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_BUSY,read);
    TEST_ASSERT_EQUAL_UINT32(NSX_PSRAM_STATUS_BUSY,write);TEST_ASSERT_FALSE(sleep);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&c,100000));
    TEST_ASSERT_TRUE(nsx_psram_can_sleep());check_record(0);
}
static void normal_sleep_wait(void) {
    prepare();static nsx_psram_client_t c; c=(nsx_psram_client_t){0};
    TEST_ASSERT_EQUAL_UINT32(0,SCB->SCR & SCB_SCR_SLEEPDEEP_Msk);
    __disable_irq();
    uint32_t status=nsx_psram_read_async(&c,SOURCE_OFFSET,SLOT(1),4096,done,&records[0]);
    if(status!=0) { __enable_irq();TEST_FAIL_MESSAGE("sleep DMA submission failed"); }
    while(!records[0].count) { __DSB();__WFI();__enable_irq();__ISB();__disable_irq(); }
    __enable_irq();
    check_record(0);TEST_ASSERT_EQUAL_MEMORY(source,SLOT(1),4096);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_pending(&c));
}
static void blocking_transitions(void) {
    prepare(); static nsx_psram_client_t client; client=(nsx_psram_client_t){0};
    const uint32_t offset=SOURCE_OFFSET+8192u;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_read_async(&client,SOURCE_OFFSET,SLOT(1),4096,done,&records[0]));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
    check_record(0);TEST_ASSERT_EQUAL_MEMORY(source,SLOT(1),4096);
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_write(offset,xip_source,4096));
    memset(SLOT(1),0x5a,4096);
    uint8_t *readback=(uint8_t *)SLOT(1)+1;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_read(offset,readback,4095));
    TEST_ASSERT_EQUAL_MEMORY(xip_source,readback,4095);
    TEST_ASSERT_EQUAL_HEX8(0x5a,readback[-1]);
    TEST_ASSERT_EQUAL_HEX8(0x5a,readback[4095]);
    prepare();client=(nsx_psram_client_t){0};
    uint32_t *last=SLOT(1)+WORDS-1;
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_read_async(&client,offset,last,4,done,&records[0]));
    TEST_ASSERT_EQUAL_UINT32(0,nsx_psram_client_drain(&client,100000));
    check_record(0);TEST_ASSERT_EQUAL_UINT32(xip_source[0],*last);
    TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a,last[-1]);
    TEST_ASSERT_EQUAL_HEX32(0x5a5a5a5a,last[1]);
}
int main(void) {
    nsx_test_begin("psram-async");
    RUN_TEST(initialize);RUN_TEST(dtcm_sizes_and_edges);RUN_TEST(queue_depth);RUN_TEST(mixed_lengths);
    RUN_TEST(xip_during_dma);RUN_TEST(two_clients);RUN_TEST(refusals);
    RUN_TEST(blocking_exclusion);RUN_TEST(blocking_transitions);RUN_TEST(normal_sleep_wait);
    nsx_test_finish();
}
