/**
 * @brief Apollo5 staged power-profile dump helpers.
 */

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power.h"

typedef struct {
    volatile uint32_t *addr;
    const char *name;
    const char *description;
} nsx_power_profile_reg_t;

#if !defined(AM_PART_APOLLO510L)
    #define NSX_POWER_PROFILE_SSRAM_CFG AM_HAL_PWRCTRL_SRAM_3M
#else
    #define NSX_POWER_PROFILE_SSRAM_CFG AM_HAL_PWRCTRL_SRAM_1P75M
#endif

static const nsx_power_profile_reg_t s_nsx_power_profile_regs[] = {
    {&PWRCTRL->MCUPERFREQ, "PWRCTRL.MCUPERFREQ", "MCU performance frequency."},
    {&PWRCTRL->DEVPWREN, "PWRCTRL.DEVPWREN", "Peripheral power enable mask."},
    {&PWRCTRL->DEVPWRSTATUS, "PWRCTRL.DEVPWRSTATUS", "Peripheral power status mask."},
    {&PWRCTRL->MEMPWREN, "PWRCTRL.MEMPWREN", "Memory power enable mask."},
    {&PWRCTRL->MEMPWRSTATUS, "PWRCTRL.MEMPWRSTATUS", "Memory power status."},
    {&PWRCTRL->MEMRETCFG, "PWRCTRL.MEMRETCFG", "Memory retention configuration."},
    {&PWRCTRL->SYSPWRSTATUS, "PWRCTRL.SYSPWRSTATUS", "System power status."},
    {&PWRCTRL->SSRAMPWREN, "PWRCTRL.SSRAMPWREN", "Shared SRAM power enable."},
    {&PWRCTRL->SSRAMPWRST, "PWRCTRL.SSRAMPWRST", "Shared SRAM power status."},
    {&PWRCTRL->SSRAMRETCFG, "PWRCTRL.SSRAMRETCFG", "Shared SRAM retention configuration."},
    {&PWRCTRL->VRCTRL, "PWRCTRL.VRCTRL", "Voltage regulator control."},
    {&PWRCTRL->VRSTATUS, "PWRCTRL.VRSTATUS", "Voltage regulator status."},
    {&PWRCTRL->SRAMCTRL, "PWRCTRL.SRAMCTRL", "SRAM control."},
    {&PWRCTRL->EMONCTRL, "PWRCTRL.EMONCTRL", "Energy monitor control."},
    {&PWRCTRL->EMONSTATUS, "PWRCTRL.EMONSTATUS", "Energy monitor status."},
    {&FPIO->EN0, "FPIO.EN0", "FPIO enable bank 0."},
    {&FPIO->EN1, "FPIO.EN1", "FPIO enable bank 1."},
    {&FPIO->EN2, "FPIO.EN2", "FPIO enable bank 2."},
    {&MCUCTRL->ACRG, "MCUCTRL.ACRG", "Analog control register group."},
    {&MCUCTRL->VRCTRL, "MCUCTRL.VRCTRL", "MCU voltage regulator control."},
    {&MCUCTRL->BODCTRL, "MCUCTRL.BODCTRL", "Brownout detector control."},
    {&MCUCTRL->XTALCTRL, "MCUCTRL.XTALCTRL", "Crystal control."},
    {&CLKGEN->OCTRL, "CLKGEN.OCTRL", "Oscillator control."},
    {&CLKGEN->CLOCKENSTAT, "CLKGEN.CLOCKENSTAT", "Clock enable status."},
    {&CLKGEN->CLOCKEN2STAT, "CLKGEN.CLOCKEN2STAT", "Clock enable 2 status."},
    {&CLKGEN->CLOCKEN3STAT, "CLKGEN.CLOCKEN3STAT", "Clock enable 3 status."},
    {&CLKGEN->MISC, "CLKGEN.MISC", "Clock generator miscellaneous state."},
    {&STIMER->STCFG, "STIMER.STCFG", "System timer configuration."},
    {&STIMER->STMINTSTAT, "STIMER.STMINTSTAT", "System timer interrupt status."},
    {&TIMER->CTRL, "TIMER.CTRL", "Timer block control."},
    {&TIMER->STATUS, "TIMER.STATUS", "Timer block status."},
    {&TIMER->GLOBEN, "TIMER.GLOBEN", "Timer global enable bits."},
    {&TIMER->INTSTAT, "TIMER.INTSTAT", "Timer interrupt status."},
};

static void nsx_power_profile_print_json(uint32_t snapshot_index) {
    size_t count = sizeof(s_nsx_power_profile_regs) / sizeof(s_nsx_power_profile_regs[0]);

    nsx_printf("{\n");
    nsx_printf("  \"snapshotIndex\": %lu,\n", (unsigned long)snapshot_index);
    nsx_printf("  \"registers\": [\n");

    for (size_t index = 0; index < count; ++index) {
        const nsx_power_profile_reg_t *reg = &s_nsx_power_profile_regs[index];
        const char *comma = (index + 1u < count) ? "," : "";

        nsx_printf("    {\"name\": \"%s\", \"value\": \"0x%08lX\", \"description\": \"%s\"}%s\n",
            reg->name,
            (unsigned long)(*reg->addr),
            reg->description,
            comma);
    }

    nsx_printf("  ]\n");
    nsx_printf("}\n");
}

static void nsx_power_profile_print_csv(void) {
    size_t count = sizeof(s_nsx_power_profile_regs) / sizeof(s_nsx_power_profile_regs[0]);

    nsx_printf("REGNAME,VALUE,DESCRIPTION\n");
    for (size_t index = 0; index < count; ++index) {
        const nsx_power_profile_reg_t *reg = &s_nsx_power_profile_regs[index];
        nsx_printf("%s,0x%08lX,%s\n",
            reg->name,
            (unsigned long)(*reg->addr),
            reg->description);
        nsx_delay_us(1000);
    }
}

uint32_t nsx_power_profile_platform_dump(
    uint32_t snapshot_index,
    nsx_power_profile_format_t format) {
    (void)snapshot_index;

    if (format == NSX_POWER_PROFILE_FORMAT_CSV) {
        nsx_power_profile_print_csv();
        return NSX_STATUS_SUCCESS;
    }

    nsx_power_profile_print_json(snapshot_index);
    return NSX_STATUS_SUCCESS;
}
