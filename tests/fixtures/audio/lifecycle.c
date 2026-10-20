#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "nsx_audio.h"
#include "am_mcu_apollo.h"

extern void am_pdm0_isr(void);
static const char *failure;
static const char *cleanup_failure;
static unsigned cleanup_failures;
static unsigned failures, calls, init_calls, dma_calls, callbacks, irq_enabled;
static uint32_t irq_status=AM_HAL_PDM_INT_DCMP;
static am_hal_pdm_transfer_t *serviced_transfer;
static bool powered, callback_stop, callback_restart;
static nsx_audio_config_t *active;
static _Alignas(32) uint32_t dma[32];
static _Alignas(32) uint32_t small_dma[8];
static int16_t pcm[16];

static unsigned operation(const char *name) {
    calls++;
    if (failure != NULL && !strcmp(failure, name) && !failures++) {
        return 91;
    }
    if (cleanup_failure != NULL && failures && !strcmp(cleanup_failure, name) && !cleanup_failures++) {
        return 92;
    }
    return 0;
}
void test_irq_enable(unsigned n) {(void)n; assert(active->_started); irq_enabled=1;}
void test_irq_disable(unsigned n) {(void)n; irq_enabled=0;}
unsigned am_hal_pdm_initialize(unsigned n, void **h) {
    (void)n; init_calls++; unsigned rc=operation("initialize");
    if (!rc) *h=(void*)1;
    return rc;
}
unsigned am_hal_pdm_power_control(void*h,unsigned mode,bool retain) {
    (void)h;(void)retain;
    unsigned rc=operation(mode==AM_HAL_PDM_POWER_ON?"power-on":"power-off");
    if(!rc)powered=mode==AM_HAL_PDM_POWER_ON;
    return rc;
}
unsigned am_hal_pdm_configure(void*h,am_hal_pdm_config_t*c) {(void)h;(void)c;assert(powered);return operation("configure");}
unsigned am_hal_pdm_fifo_flush(void*h) {(void)h;assert(powered);return operation("flush");}
unsigned am_hal_pdm_fifo_threshold_setup(void*h,unsigned n) {(void)h;(void)n;assert(powered);return operation("threshold");}
unsigned am_hal_pdm_interrupt_enable(void*h,unsigned n) {(void)h;(void)n;assert(powered);return operation("irq-enable");}
unsigned am_hal_pdm_interrupt_disable(void*h,unsigned n) {(void)h;(void)n;assert(powered);return operation("irq-disable");}
unsigned am_hal_pdm_interrupt_clear(void*h,unsigned n) {(void)h;(void)n;assert(powered);return operation("irq-clear");}
unsigned am_hal_pdm_interrupt_status_get(void*h,uint32_t*s,bool b) {(void)h;(void)b;assert(powered);*s=irq_status;return operation("status");}
unsigned am_hal_pdm_enable(void*h) {(void)h;assert(powered);return operation("enable");}
unsigned am_hal_pdm_disable(void*h) {(void)h;assert(powered);return operation("disable");}
unsigned am_hal_pdm_deinitialize(void*h) {(void)h;assert(!powered);return operation("deinit");}
unsigned am_hal_pdm_dma_start(void*h,am_hal_pdm_transfer_t*t) {
    (void)h;assert(powered);assert(t->ui32TotalCount==active->num_samples*4);
    assert(t->ui32TotalCount*2<=active->dma_buffer_size);dma_calls++;
    return operation("dma-start");
}
unsigned am_hal_pdm_dma_stop(void*h) {(void)h;assert(powered);return operation("dma-stop");}
unsigned am_hal_clkmgr_clock_config(unsigned id,unsigned hz,void*c) {(void)id;(void)hz;(void)c;return operation("clock");}
void am_bsp_pdm_pins_enable(unsigned n) {(void)n;}
void am_bsp_pdm_pins_disable(unsigned n) {(void)n;}
unsigned am_hal_pdm_dma_get_buffer(void*h) {(void)h;return (unsigned)(uintptr_t)dma;}
unsigned am_hal_pdm_interrupt_service(void*h,am_hal_interupt_service_msg_t*s,am_hal_pdm_transfer_t*t) {
    (void)h;s->isrStat=irq_status;t->ui32BufferWithDataAddr=(unsigned)(uintptr_t)dma;
    serviced_transfer=t;
    return operation("service");
}
static void callback(nsx_audio_config_t*c,void*b,uint32_t n) {
    assert(b==pcm && n==16);callbacks++;
    if(callback_stop)assert(nsx_audio_stop(c)==0);
    if(callback_restart) {
        assert(nsx_audio_stop(c)==0);
        c->num_samples=4;
        c->dma_buffer=small_dma;
        c->dma_buffer_size=sizeof small_dma;
        assert(nsx_audio_init(c)==0 && nsx_audio_start(c)==0);
    }
}

int main(int argc,char **argv) {
    assert(argc==3);
    nsx_audio_config_t cfg={.source=NSX_AUDIO_SOURCE_PDM,.num_channels=1,.num_samples=16,
        .dma_buffer=dma,.dma_buffer_size=sizeof dma,.pcm_buffer=pcm,.pcm_buffer_size=sizeof pcm,
        .callback=callback,.pdm={.clock=NSX_AUDIO_CLK_HFRC,.sample_width=NSX_AUDIO_PDM_16BIT}};
#ifndef TEST_APOLLO3
    cfg.pdm.clock=NSX_AUDIO_CLK_HFRC2_ADJ;
#endif
    active=&cfg;
    assert((uintptr_t)dma<=UINT32_MAX);
    assert(nsx_audio_get_status(NULL)!=0);
    assert(nsx_audio_get_status(&cfg)!=0);
    const char *phase=argv[1], *fault=argv[2];
    if(!strcmp(phase,"init-cleanup")) {
        failure=!strcmp(fault,"power-on/power-off")?"power-on":"configure";
        cleanup_failure=!strcmp(fault,"configure/deinit")?"deinit":"power-off";
        assert(nsx_audio_init(&cfg)!=0);
        assert(failures && cleanup_failures && cfg._pdm_handle!=NULL);
        assert(!cfg._started && !irq_enabled && nsx_audio_get_status(&cfg)!=0);
        unsigned before=calls;
        nsx_audio_config_t other=cfg;
        assert(nsx_audio_init(&cfg)!=0 && nsx_audio_init(&other)!=0);
        assert(nsx_audio_start(&cfg)!=0 && calls==before);
        failure=NULL;
        cleanup_failure=NULL;
        assert(nsx_audio_stop(&cfg)==0);
        assert(!powered && cfg._pdm_handle==NULL);
        assert(nsx_audio_init(&other)==0 && nsx_audio_stop(&other)==0);
        return 0;
    }
    if(!strcmp(phase,"init")) {
        failure=fault;
        assert(nsx_audio_init(&cfg)!=0);
        assert(!cfg._started && cfg._pdm_handle==NULL && !irq_enabled);
        assert(failures && !powered);
        if(!strcmp(fault,"clock"))assert(init_calls==0);
        failure=NULL;
        assert(nsx_audio_init(&cfg)==0);
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    assert(nsx_audio_init(&cfg)==0);
    assert(!irq_enabled && !cfg._started && nsx_audio_get_status(&cfg)==0);
    if(!strcmp(phase,"start")) {
        failure=fault;
        assert(nsx_audio_start(&cfg)!=0);
        assert(!cfg._started && !irq_enabled && nsx_audio_get_status(&cfg)!=0);
        if(!strcmp(fault,"enable"))assert(dma_calls==0);
        assert(nsx_audio_start(&cfg)!=0); /* stop/reinit required */
        assert(nsx_audio_stop(&cfg)==0);
        assert(nsx_audio_get_status(&cfg)!=0);
        failure=NULL;
        assert(nsx_audio_init(&cfg)==0 && nsx_audio_start(&cfg)==0);
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    assert(nsx_audio_start(&cfg)==0 && irq_enabled);
    if(!strcmp(phase,"hardware-error")) {
        if(strstr(fault,"derr"))irq_status=AM_HAL_PDM_INT_DERR;
        else if(strstr(fault,"undfl"))irq_status=AM_HAL_PDM_INT_UNDFL;
        else {assert(strstr(fault,"ovf"));irq_status=AM_HAL_PDM_INT_OVF;}
        if(strstr(fault,"+dcmp"))irq_status|=AM_HAL_PDM_INT_DCMP;
        am_pdm0_isr();
        assert(!cfg._started && !irq_enabled && nsx_audio_get_status(&cfg)!=0);
        assert(callbacks==0 && dma_calls==1);
#ifdef TEST_APOLLO3
        assert(serviced_transfer!=NULL && !serviced_transfer->bRestartDMA);
#endif
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    if(!strcmp(phase,"stop")) {
        failure=fault;
        assert(nsx_audio_stop(&cfg)!=0);
        assert(cfg._pdm_handle!=NULL && !cfg._started && !irq_enabled);
        assert(nsx_audio_get_status(&cfg)!=0);
        assert(nsx_audio_start(&cfg)!=0);
        assert(nsx_audio_stop(&cfg)==0);
        assert(!powered && cfg._pdm_handle==NULL);
        return 0;
    }
    if(!strcmp(phase,"isr")) {
        failure=fault;
        am_pdm0_isr();
        assert(!cfg._started && !irq_enabled && nsx_audio_get_status(&cfg)!=0);
        assert(callbacks==(!strcmp(fault,"dma-start")?1u:0u));
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    if(!strcmp(phase,"ownership")) {
        unsigned before=calls;
        assert(nsx_audio_init(&cfg)!=0);
        nsx_audio_config_t other=cfg;
        assert(nsx_audio_init(&other)!=0 && nsx_audio_start(&other)!=0 && nsx_audio_stop(&other)!=0);
        assert(nsx_audio_start(&cfg)!=0 && calls==before && cfg._started);
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    if(!strcmp(phase,"callback-restart")) {
        callback_restart=true;
        am_pdm0_isr();
        assert(callbacks==1 && cfg._started && irq_enabled && powered && dma_calls==2);
        assert(nsx_audio_stop(&cfg)==0);
        return 0;
    }
    assert(!strcmp(phase,"callback-stop"));
    callback_stop=true;
    am_pdm0_isr();
    assert(callbacks==1 && !cfg._started && !irq_enabled && !powered && dma_calls==1);
    return 0;
}
