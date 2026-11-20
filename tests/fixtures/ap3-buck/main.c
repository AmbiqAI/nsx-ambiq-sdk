/* Keep the released register encodings and HAL signatures for each MCU. */
#include "am_mcu_apollo.h"
#include <assert.h>

static struct { uint32_t CTRL0, AUX0; } pairs[AM_HAL_CTIMER_TIMERS_NUM];
static struct { uint32_t INTEN, INTSTAT, STCFG; } block;
static struct { uint32_t SUPPLYSTATUS; } power;
static struct { struct { uint32_t BURSTSTATUS; } FREQCTRL_b; } clockgen;
#undef CTIMERn
#undef CTIMER
#undef PWRCTRL
#undef CLKGEN
#define CTIMERn(n) (&pairs[n])
#define CTIMER (&block)
#define PWRCTRL (&power)
#define CLKGEN (&clockgen)

#include "apollo3/nsx_power_buck_ctimer.c"

static uint32_t control;
static unsigned starts, stops, clears;

void am_hal_ctimer_config_single(uint32_t timer, uint32_t segment, uint32_t config) {
    assert(timer == 4 && segment == AM_HAL_CTIMER_BOTH);
    control = config;
    pairs[timer].CTRL0 = config | AM_HAL_CTIMER_LINK;
}
void am_hal_ctimer_start(uint32_t timer, uint32_t segment) {
    assert(timer == 4 && segment == AM_HAL_CTIMER_BOTH);
    starts++;
}
void am_hal_ctimer_stop(uint32_t timer, uint32_t segment) {
    assert(timer == 4 && segment == AM_HAL_CTIMER_BOTH);
    stops++;
}
void am_hal_ctimer_clear(uint32_t timer, uint32_t segment) {
    assert(timer == 4 && segment == AM_HAL_CTIMER_BOTH);
    clears++;
}
uint32_t am_hal_ctimer_read(uint32_t timer, uint32_t segment) {
    assert(timer == 4 && segment == AM_HAL_CTIMER_BOTH);
    return 0x1234abcd;
}
uint32_t am_hal_stimer_counter_get(void) { return 0x98761234; }

int main(void) {
    assert(nsx_power_buck_rails() == (NSX_BUCK_RAIL(NSX_BUCK_CPU) | NSX_BUCK_RAIL(NSX_BUCK_MEMORY) |
                                      NSX_BUCK_RAIL(NSX_BUCK_BLE)));
    assert(nsx_buck_hw_check(NSX_BUCK_VDDF, 4) == NSX_BUCK_UNSUPPORTED);
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, 8) == NSX_BUCK_INVALID);
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, AM_HAL_ENTROPY_CTIMER) == NSX_BUCK_BUSY);

    pairs[4].CTRL0 = CTIMER_CTRL4_TMRB4EN_Msk;
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, 4) == NSX_BUCK_BUSY);
    pairs[4].CTRL0 = 0;
    pairs[4].AUX0 = CTIMER_AUX4_TMRA4TRIG_Msk;
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, 4) == NSX_BUCK_BUSY);
    pairs[4].AUX0 = 0;
    block.INTEN = AM_HAL_CTIMER_INT_TIMERB4C1;
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, 4) == NSX_BUCK_BUSY);
    block.INTEN = AM_HAL_CTIMER_INT_TIMERB5C1;
    block.INTSTAT = AM_HAL_CTIMER_INT_TIMERA4C0;
    assert(nsx_buck_hw_check(NSX_BUCK_CPU, 4) == NSX_BUCK_BUSY);
    block.INTSTAT = 0;

    const nsx_buck_rail_t rails[] = {NSX_BUCK_CPU, NSX_BUCK_MEMORY, NSX_BUCK_BLE};
    const uint32_t sels[] = {CTIMER_CTRL4_TMRA4CLK_BUCKA, CTIMER_CTRL4_TMRA4CLK_BUCKB, CTIMER_CTRL4_TMRA4CLK_BUCKBLE};
    for (unsigned i = 0; i < 3; ++i) {
        bool touched = false;
        assert(nsx_buck_hw_check(rails[i], 4) == NSX_BUCK_OK);
        assert(nsx_buck_hw_start(rails[i], 4, &touched) == NSX_BUCK_OK && touched);
        assert(control == (_VAL2FLD(CTIMER_CTRL4_TMRA4CLK, sels[i]) | AM_HAL_CTIMER_FN_CONTINUOUS));
        assert(nsx_buck_hw_count(4) == 0x1234abcd);
        assert(nsx_buck_hw_stop(4) == NSX_BUCK_OK);
    }
    assert(starts == 3 && stops == 3 && clears == 3);
    assert(block.INTEN == AM_HAL_CTIMER_INT_TIMERB5C1);

    uint32_t tick;
    block.STCFG = AM_HAL_STIMER_XTAL_32KHZ;
    assert(nsx_buck_hw_time(&tick) == 32768 && tick == 0x98761234);
    block.STCFG |= AM_HAL_STIMER_CFG_FREEZE;
    assert(nsx_buck_hw_time(&tick) == 0);
    block.STCFG = AM_HAL_STIMER_XTAL_1KHZ;
    assert(nsx_buck_hw_time(&tick) == 1024);
    block.STCFG = AM_HAL_STIMER_LFRC_1KHZ;
    assert(nsx_buck_hw_time(&tick) == 0);
    block.STCFG = AM_HAL_STIMER_HFRC_CTIMER0A;
    assert(nsx_buck_hw_time(&tick) == 0);
    power.SUPPLYSTATUS = PWRCTRL_SUPPLYSTATUS_BLEBUCKON_Msk;
    clockgen.FREQCTRL_b.BURSTSTATUS = 1;
    nsx_buck_modes_t modes = nsx_buck_hw_modes();
    assert(modes.main_supply == NSX_BUCK_SUPPLY_OFF && modes.ble_supply == NSX_BUCK_SUPPLY_ON);
    assert(modes.cpu_mode == NSX_BUCK_CPU_BURST);
    return 0;
}
