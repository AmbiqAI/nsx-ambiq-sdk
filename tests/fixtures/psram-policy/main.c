#include <assert.h>
#include <string.h>
#include "nsx_psram_dma.h"
#include "nsx_psram_internal.h"

#define ORIGINAL_BURST 0xa5000020u
#define ORIGINAL_THRESHOLD 0x5a00080cu
#define RECEIVE_BURST 0xa5000040u
#define RECEIVE_THRESHOLD 0x5a000f0cu

static MSPI0_Type registers;
static unsigned irq, submissions, queries, completions, releases;
static uint32_t clock_status, hardware_status, submit_status;
static bool tip, expect_receive;
static uint32_t cq_entries;
static uint8_t handle;
static uint32_t destination[1024] __attribute__((aligned(4096)));
typedef struct {
    am_hal_mspi_dma_transfer_t transfer;
    am_hal_mspi_callback_t done;
    void *ctx;
} request_t;
static request_t queue[8];
static unsigned queued;

MSPI0_Type *test_mspi(unsigned module) { assert(module == 0u); return &registers; }
uint32_t am_hal_interrupt_master_disable(void) { unsigned old=irq;irq=1;return old; }
void am_hal_interrupt_master_set(uint32_t state) { irq=state; }
void test_dmb(void) {}
void test_dsb(void) { assert(irq && !tip && !cq_entries); }
void nsx_psram_mark_failed(void) { assert(!"unexpected module failure"); }
uint32_t am_hal_clkmgr_clock_request(unsigned clock, am_hal_clkmgr_user_id_e user) {
    assert(irq && clock == AM_HAL_CLKMGR_CLK_ID_HFRC && user == AM_HAL_CLKMGR_USER_ID_MSPI0);
    return clock_status;
}
uint32_t am_hal_clkmgr_clock_release(unsigned clock, am_hal_clkmgr_user_id_e user) {
    assert(irq && clock == AM_HAL_CLKMGR_CLK_ID_HFRC && user == AM_HAL_CLKMGR_USER_ID_MSPI0);
    ++releases;return 0;
}
uint32_t am_hal_mspi_status_get(void *opaque, am_hal_mspi_status_t *status) {
    assert(irq && opaque == &handle);++queries;
    status->bTIP=tip;status->ui32NumCQEntries=cq_entries;
    return hardware_status;
}
uint32_t am_hal_mspi_nonblocking_transfer(void *opaque, void *transfer, unsigned mode,
                                        am_hal_mspi_callback_t done, void *ctx) {
    assert(irq && opaque == &handle && mode == AM_HAL_MSPI_TRANS_DMA);
    ++submissions;
    assert(registers.DMABCOUNT == (expect_receive ? RECEIVE_BURST : ORIGINAL_BURST));
    assert(registers.DMATHRESH == (expect_receive ? RECEIVE_THRESHOLD : ORIGINAL_THRESHOLD));
    if (submit_status) return submit_status;
    assert(queued < sizeof(queue)/sizeof(queue[0]));
    queue[queued++]=(request_t){*(am_hal_mspi_dma_transfer_t *)transfer,done,ctx};
    tip=true;++cq_entries;return 0;
}
static void finish(void) {
    assert(!irq);
    for(unsigned i=0;i<queued;++i) {
        request_t *request=&queue[i];
        uint8_t *buffer=(void *)(uintptr_t)request->transfer.ui32SRAMAddress;
        if(request->transfer.eDirection == AM_HAL_MSPI_RX)
            memset(buffer,0x6c,request->transfer.ui32TransferCount);
        else
            for(unsigned j=0;j<request->transfer.ui32TransferCount;++j) assert(buffer[j] == 0xa7);
        if(request->done) request->done(request->ctx,0);
    }
    queued=0;tip=false;cq_entries=0;
}
void am_hal_delay_us(uint32_t us) { assert(us == 1u);if(!irq && queued) finish(); }
static void completed(void *ctx, uint32_t status) { assert(ctx == &handle && status == 0);++completions; }
static am_hal_mspi_dma_transfer_t read_request(void) {
    return (am_hal_mspi_dma_transfer_t){.eDirection=AM_HAL_MSPI_RX,
        .ui32TransferCount=4,.ui32SRAMAddress=(uint32_t)(uintptr_t)destination};
}
static void submit(bool pending) {
    am_hal_mspi_dma_transfer_t transfer=read_request();irq=1;
    assert(nsx_psram_dma_submit(&transfer,pending,completed,&handle) == 0);
    irq=0;
}
static void original(void) {
    assert(registers.DMABCOUNT == ORIGINAL_BURST && registers.DMATHRESH == ORIGINAL_THRESHOLD);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    registers=(MSPI0_Type){ORIGINAL_BURST,ORIGINAL_THRESHOLD};
    nsx_psram_dma_configure(&handle,48000000u);
    if(!strcmp(argv[1],"switching")) {
        expect_receive=true;submit(false);unsigned first_queries=queries;
        submit(true);assert(queries == first_queries && completions == 0);
        finish();assert(completions == 2);
        expect_receive=false;
        uint8_t bytes[16];memset(bytes,0xa5,sizeof(bytes));
        assert(nsx_psram_dma_read(0,bytes+1,13) == 0);original();
        assert(bytes[0] == 0xa5 && bytes[14] == 0xa5);
        for(unsigned i=1;i<14;++i) assert(bytes[i] == 0x6c);
        memset(bytes,0xa7,sizeof(bytes));assert(nsx_psram_dma_write(0,bytes,sizeof(bytes)) == 0);original();
        expect_receive=true;submit(false);finish();
        registers=(MSPI0_Type){ORIGINAL_BURST,ORIGINAL_THRESHOLD};
        submit(false);finish();assert(completions == 4 && queries > first_queries);
        assert(!releases);return 0;
    }
    if(!strcmp(argv[1],"refusals")) {
        am_hal_mspi_dma_transfer_t transfer=read_request();irq=1;
        clock_status=42;assert(nsx_psram_dma_submit(&transfer,false,completed,&handle) == 42);
        original();assert(!queries && !submissions);
        clock_status=0;hardware_status=55;
        assert(nsx_psram_dma_submit(&transfer,false,completed,&handle) == 55);original();
        hardware_status=0;tip=true;
        assert(nsx_psram_dma_submit(&transfer,false,completed,&handle) == AM_HAL_STATUS_INVALID_OPERATION);original();
        tip=false;cq_entries=1;
        assert(nsx_psram_dma_submit(&transfer,false,completed,&handle) == AM_HAL_STATUS_INVALID_OPERATION);original();
        cq_entries=0;unsigned prior_queries=queries;
        assert(nsx_psram_dma_submit(&transfer,true,completed,&handle) == NSX_PSRAM_STATUS_BUSY);
        original();assert(queries == prior_queries && !submissions && !completions);
        submit_status=77;expect_receive=true;
        assert(nsx_psram_dma_submit(&transfer,false,completed,&handle) == 77);
        assert(!queued && !completions && !releases);return 0;
    }
    if(!strcmp(argv[1],"clock-gate")) {
        const uint32_t clocks[]={125000000u,192000000u,250000000u};
        for(unsigned i=0;i<sizeof(clocks)/sizeof(clocks[0]);++i) {
            nsx_psram_dma_configure(&handle,clocks[i]);submit(false);finish();original();
        }
        assert(!queries);nsx_psram_dma_configure(&handle,96000000u);
        expect_receive=true;submit(false);finish();assert(queries == 1);return 0;
    }
    assert(!strcmp(argv[1],"unqualified"));submit(false);finish();original();assert(!queries);
    return 0;
}
