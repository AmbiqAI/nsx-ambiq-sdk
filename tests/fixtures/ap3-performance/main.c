#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "nsx_performance_internal.h"

uint32_t SystemCoreClock=777;
clock_regs clock_hw;
power_regs power_hw={.MISC_b={1},.SUPPLYSTATUS_b={1},.SUPPLYSRC_b={1}};
mcu_regs mcu_hw={.SKU_b={1}};
static unsigned inits, transitions, fault;
uint32_t am_hal_burst_mode_initialize(am_hal_burst_avail_e *available) {
    ++inits;
    if(fault==1) return 17;
    if(fault==2) return 0; /* Success without an output is still failure. */
    *available=fault==3 ? AM_HAL_BURST_NOTAVAIL : AM_HAL_BURST_AVAIL;
    clock_hw.FREQCTRL_b.BURSTREQ=clock_hw.FREQCTRL_b.BURSTSTATUS=0;
    return 0;
}
static uint32_t transition(am_hal_burst_mode_e *mode, bool high) {
    ++transitions;
    if(fault==4) return 19;
    if(fault!=5) *mode=(high ^ (fault==6)) ? AM_HAL_BURST_MODE : AM_HAL_NORMAL_MODE;
    clock_hw.FREQCTRL_b.BURSTREQ=high ^ (fault==7);
    clock_hw.FREQCTRL_b.BURSTSTATUS=high ^ (fault==8);
    if(fault==9) clock_hw.CCTRL_b.CORESEL^=1;
    return 0;
}
uint32_t am_hal_burst_mode_enable(am_hal_burst_mode_e *mode) {return transition(mode,true);}
uint32_t am_hal_burst_mode_disable(am_hal_burst_mode_e *mode) {return transition(mode,false);}
static uint32_t select(bool power, bool high) {
    return power ? nsx_power_set_performance_mode(high ? NSX_POWER_PERF_HIGH : NSX_POWER_PERF_LOW) :
                   nsx_performance_select(high ? NSX_PERF_HIGH : NSX_PERF_LOW);
}
int main(int argc,char **argv) {
    assert(argc==4);
    const char *scenario=argv[1]; bool power=atoi(argv[2]); unsigned option=atoi(argv[3]);
    if(!strcmp(scenario,"invalid")) {
        assert(nsx_power_set_performance_mode(NSX_POWER_PERF_NOT_SET)!=0);
        assert(nsx_power_set_performance_mode((nsx_power_perf_mode_t)99)!=0);
        assert(nsx_performance_select((nsx_perf_mode_e)99)!=0);
        assert(inits==0 && transitions==0 && SystemCoreClock==777);
    } else if(!strcmp(scenario,"fpga")) {
        assert(select(power,false)==AM_HAL_STATUS_INVALID_OPERATION);
        assert(select(power,true)==AM_HAL_STATUS_INVALID_OPERATION);
        assert(inits==0 && transitions==0 && SystemCoreClock==777);
    } else if(!strcmp(scenario,"normal")) {
        clock_hw.CCTRL_b.CORESEL=option;
        mcu_hw.SKU_b.ALLOWBURST=0; power_hw.SUPPLYSRC_b.BLEBUCKEN=0;
        assert(select(power,false)==0 && inits==0 && transitions==0);
        assert(SystemCoreClock==(option ? 24000000u : 48000000u));
    } else if(!strcmp(scenario,"preflight")) {
        if(option==0) mcu_hw.SKU_b.ALLOWBURST=0;
        if(option==1) power_hw.SUPPLYSRC_b.BLEBUCKEN=0;
        if(option==2) {
            power_hw.SUPPLYSTATUS_b.SIMOBUCKON=0;
#if defined(AM_PART_APOLLO3)
            assert(select(power,true)==0); return 0;
#endif
        }
        if(option>=3) clock_hw.CCTRL_b.CORESEL=option-2;
        assert(select(power,true)!=0 && inits==0 && transitions==0 && SystemCoreClock==777);
    } else if(!strcmp(scenario,"init-fault")) {
        fault=option;
        assert(select(power,true)!=0 && inits==1 && transitions==0 && SystemCoreClock==777);
        fault=0;
        assert(select(!power,true)==0 && inits==2 && transitions==1 && SystemCoreClock==96000000);
    } else if(!strcmp(scenario,"transition-fault")) {
        bool high=option<10; fault=high ? option : option-10;
        if(!high) clock_hw.FREQCTRL_b.BURSTREQ=clock_hw.FREQCTRL_b.BURSTSTATUS=1;
        assert(select(power,high)!=0 && transitions==1 && SystemCoreClock==777);
    } else if(!strcmp(scenario,"mixed")) {
        if(option) power_hw.MISC_b.SIMOBUCKEN=power_hw.SUPPLYSTATUS_b.SIMOBUCKON=0;
        assert(select(power,true)==0 && inits==1 && transitions==1);
        assert(SystemCoreClock==96000000);
        assert(select(!power,true)==0 && inits==1 && transitions==1);
        assert(nsx_performance_select(NSX_PERF_MEDIUM)==0);
        assert(nsx_power_set_performance_mode(NSX_POWER_PERF_MAX)==0);
        assert(inits==1 && transitions==1);
        assert(select(!power,false)==0 && inits==1 && transitions==2 && SystemCoreClock==48000000);
        assert(select(power,false)==0 && transitions==2);
        assert(select(power,true)==0 && inits==1 && transitions==3 && SystemCoreClock==96000000);
    } else assert(!"unknown case");
    return 0;
}
