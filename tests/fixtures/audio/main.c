#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "nsx_audio.h"
#include "am_mcu_apollo.h"
extern void am_pdm0_isr(void);
static unsigned init_clock_error,enable_error,dma_error,dma_calls,last_dma_count,last_dma_base,last_dma_reverse,init_calls,callback_count;
unsigned am_hal_pdm_initialize(unsigned n,void **h){(void)n;init_calls++;*h=(void*)1;return 0;}
unsigned am_hal_pdm_power_control(void*h,unsigned p,bool r){(void)h;(void)p;(void)r;return 0;}
unsigned am_hal_pdm_configure(void*h,am_hal_pdm_config_t*c){(void)h;(void)c;return 0;}
unsigned am_hal_pdm_fifo_flush(void*h){(void)h;return 0;}
unsigned am_hal_pdm_fifo_threshold_setup(void*h,unsigned n){(void)h;(void)n;return 0;}
unsigned am_hal_pdm_interrupt_enable(void*h,unsigned n){(void)h;(void)n;return 0;}
unsigned am_hal_pdm_interrupt_disable(void*h,unsigned n){(void)h;(void)n;return 0;}
unsigned am_hal_pdm_interrupt_clear(void*h,unsigned n){(void)h;(void)n;return 0;}
unsigned am_hal_pdm_interrupt_status_get(void*h,uint32_t*s,bool b){(void)h;(void)b;*s=AM_HAL_PDM_INT_DCMP;return 0;}
unsigned am_hal_pdm_enable(void*h){(void)h;return enable_error;}
unsigned am_hal_pdm_disable(void*h){(void)h;return 0;}
unsigned am_hal_pdm_deinitialize(void*h){(void)h;return 0;}
unsigned am_hal_pdm_dma_start(void*h,am_hal_pdm_transfer_t*t){(void)h;dma_calls++;last_dma_count=t->ui32TotalCount;last_dma_base=t->ui32TargetAddr;last_dma_reverse=t->ui32TargetAddrReverse;return dma_error;}
unsigned am_hal_clkmgr_clock_config(unsigned id,unsigned hz,void*c){(void)id;(void)hz;(void)c;return init_clock_error;}
void am_bsp_pdm_pins_enable(unsigned n){(void)n;}void am_bsp_pdm_pins_disable(unsigned n){(void)n;}
static _Alignas(32) uint32_t dma[256];
static int16_t pcm[128];
unsigned am_hal_pdm_dma_get_buffer(void*h){(void)h;return (unsigned)(uintptr_t)dma;}
unsigned am_hal_pdm_interrupt_service(void*h,am_hal_interupt_service_msg_t*s,am_hal_pdm_transfer_t*t){
 (void)h;s->isrStat=AM_HAL_PDM_INT_DCMP;t->ui32BufferWithDataAddr=(unsigned)(uintptr_t)dma;return 0;}

static void cb(nsx_audio_config_t*c,void*b,uint32_t n){(void)c;assert(b==pcm);assert(n==16);callback_count++;}
int main(int argc,char**argv){
 assert(argc==2);
 nsx_audio_config_t cfg={.source=NSX_AUDIO_SOURCE_PDM,.num_channels=1,.num_samples=16,.callback=cb,.dma_buffer=dma,.dma_buffer_size=128,.pcm_buffer=pcm,.pcm_buffer_size=32,.pdm={.clock=NSX_AUDIO_CLK_HFRC,.sample_width=NSX_AUDIO_PDM_16BIT}};
 assert((uintptr_t)dma<=UINT32_MAX);
 for(unsigned i=0;i<256;i++)dma[i]=i+100;
 for(unsigned i=0;i<128;i++)pcm[i]=-1;
 const char *test=argv[1];
 bool valid=!strcmp(test,"valid") || !strcmp(test,"stereo") || !strcmp(test,"24bit") || !strcmp(test,"count-limit");
 if(!strcmp(test,"overflow"))cfg.num_samples=0x80000001u;
 if(!strcmp(test,"channel-overflow"))cfg.num_samples=0x80000001u,cfg.num_channels=2;
 if(!strcmp(test,"zero"))cfg.num_samples=0;
 if(!strcmp(test,"channels"))cfg.num_channels=3;
 if(!strcmp(test,"width"))cfg.pdm.sample_width=0;
 if(!strcmp(test,"unknown-width"))cfg.pdm.sample_width=(nsx_audio_pdm_sample_width_e)4;
 if(!strcmp(test,"high-address")){
#if UINTPTR_MAX > UINT32_MAX
 cfg.dma_buffer=(uint32_t*)(uintptr_t)0x100000000ULL;
#else
 return 0;
#endif
 }
 if(!strcmp(test,"dma-short"))cfg.dma_buffer_size--;
 if(!strcmp(test,"pcm-short"))cfg.pcm_buffer_size--;
 if(!strcmp(test,"dma-align"))cfg.dma_buffer=(uint32_t*)((uintptr_t)dma+4);
 if(!strcmp(test,"pcm-align"))cfg.pcm_buffer=(int16_t*)((uintptr_t)pcm+1);
 if(!strcmp(test,"address-wrap"))cfg.dma_buffer=(uint32_t*)(uintptr_t)0xffffffe0u;
 if(!strcmp(test,"instance"))cfg.pdm.mic=(nsx_audio_pdm_mic_e)99;
 if(!strcmp(test,"count-too-large"))cfg.num_samples=262144,cfg.dma_buffer_size=UINT32_MAX,cfg.pcm_buffer_size=UINT32_MAX;
 if(!strcmp(test,"count-limit"))cfg.num_samples=262143,cfg.dma_buffer_size=UINT32_MAX,cfg.pcm_buffer_size=UINT32_MAX;
 if(!strcmp(test,"stereo"))cfg.num_channels=2,cfg.num_samples=8;
 if(!strcmp(test,"24bit"))cfg.pdm.sample_width=NSX_AUDIO_PDM_24BIT,cfg.dma_buffer_size=192;
#ifdef TEST_APOLLO3
 if(!strcmp(test,"24bit"))valid=false;
#endif
 unsigned init=nsx_audio_init(&cfg);
 if(!valid){assert(init!=0);assert(init_calls==0);return 0;}
 assert(init==0);assert(init_calls==1);
 if(!strcmp(test,"count-limit"))return 0;
 assert(nsx_audio_start(&cfg)==0);
 assert(dma_calls==1);
 unsigned frame=!strcmp(test,"24bit")?96:64;
 assert(last_dma_count==frame);
 assert(last_dma_base==(unsigned)(uintptr_t)dma);
 assert(last_dma_reverse==last_dma_base+frame);
 if(!strcmp(test,"24bit"))for(unsigned i=0;i<256;i++)dma[i]=(i+100)<<8;
 am_pdm0_isr();
 assert(callback_count==1);
#ifdef TEST_APOLLO3
 assert(dma_calls==1); /* Apollo3 HAL interrupt_service owns auto-restart. */
#else
 assert(dma_calls==2);
#endif
 assert(last_dma_count==frame);
 assert(last_dma_base==(unsigned)(uintptr_t)dma);
 assert(last_dma_reverse==last_dma_base+frame);
 for(unsigned i=0;i<16;i++)assert(pcm[i]==(int)(i+100));
 assert(pcm[16]==-1);
 assert(nsx_audio_stop(&cfg)==0);
 return 0;
}
