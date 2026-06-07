/**
 * @brief Apollo4 TempCo helper for nsx-power
 */

#if !defined(AM_PART_APOLLO4L)

#include <stdbool.h>

#include "am_bsp.h"
#include "am_mcu_apollo.h"
#include "am_util.h"
#include "nsx_core.h"
#include "nsx_power.h"
#include "nsx_timer.h"

#define NSX_POWERTEMPCO_ADC_TEMPERATURE_SLOT 5u
#ifndef NSX_POWERTEMPCO_PERIOD_US
#define NSX_POWERTEMPCO_PERIOD_US (10u * 1024u * 1024u)
#endif
#define NSX_POWERTEMPCO_MIN_TRIM_REV 6u

#ifdef NSX_POWERTEMPCO_DIAGNOSTICS
#define NSX_POWERTEMPCO_LOG(...) nsx_printf(__VA_ARGS__)
#else
#define NSX_POWERTEMPCO_LOG(...) ((void)0)
#endif

typedef struct {
    void *adc_handle;
    uint32_t adc_slot;
    uint32_t last_handler_status;
    uint32_t callback_count;
    am_hal_adc_sample_t samples[AM_HAL_TEMPCO_NUMSAMPLES];
    bool initialized;
    bool wanted;
} nsx_power_tempco_state_t;

static void nsx_power_tempco_callback(nsx_timer_config_t *cfg);
static uint32_t nsx_power_tempco_capture_samples(
    nsx_power_tempco_state_t *state,
    uint32_t num_samples,
    am_hal_adc_sample_t samples[]);
static void nsx_power_tempco_trigger_wait(nsx_power_tempco_state_t *state);

static nsx_power_tempco_state_t s_nsx_power_tempco;
static nsx_timer_config_t s_nsx_power_tempco_timer = {
    .api = &nsx_timer_current_version,
    .timer = NSX_TIMER_TEMPCO,
    .enableInterrupt = true,
    .periodInMicroseconds = NSX_POWERTEMPCO_PERIOD_US,
    .callback = nsx_power_tempco_callback,
};

static bool nsx_power_tempco_supported(void) {
    uint32_t trim_rev = 0;
    uint32_t status = am_hal_mram_info_read(1, AM_REG_INFO1_TRIM_REV_O / 4, 1, &trim_rev);

    NSX_POWERTEMPCO_LOG("TempCo trim read: status=%lu trim_rev=%lu\n",
        (unsigned long)status,
        (unsigned long)trim_rev);

    return status == AM_HAL_STATUS_SUCCESS && trim_rev != 0xFFFFFFFFu && trim_rev >= NSX_POWERTEMPCO_MIN_TRIM_REV;
}

uint32_t nsx_power_tempco_enable(void) {
    am_hal_adc_config_t adc_cfg = {
        .eClock = AM_HAL_ADC_CLKSEL_HFRC_24MHZ,
        .ePolarity = AM_HAL_ADC_TRIGPOL_RISING,
        .eTrigger = AM_HAL_ADC_TRIGSEL_SOFTWARE,
        .eClockMode = AM_HAL_ADC_CLKMODE_LOW_POWER,
        .ePowerMode = AM_HAL_ADC_LPMODE1,
        .eRepeat = AM_HAL_ADC_SINGLE_SCAN,
        .eRepeatTrigger = AM_HAL_ADC_RPTTRIGSEL_TMR,
    };
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;
    uint32_t status;

    if (state->initialized) {
        state->wanted = true;
        NSX_POWERTEMPCO_LOG("TempCo already initialized\n");
        return NSX_STATUS_SUCCESS;
    }

    if (!nsx_power_tempco_supported()) {
        nsx_low_power_printf("WARNING TempCo trim support is unavailable on this Apollo4 device.\n");
        return NSX_STATUS_FAILURE;
    }

    state->adc_slot = NSX_POWERTEMPCO_ADC_TEMPERATURE_SLOT;

    status = am_hal_adc_initialize(0, &state->adc_handle);
    if (status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("tempco_init() Error - reservation of the ADC instance failed.\n");
        return status;
    }

    status = am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_WAKE, false);
    if (status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("tempco_init() Error - ADC power on failed.\n");
        return status;
    }

    status = am_hal_adc_configure(state->adc_handle, &adc_cfg);
    if (status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("tempco_init() Error - configuring ADC failed.\n");
        return status;
    }

    status = nsx_timer_init(&s_nsx_power_tempco_timer);
    if (status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("tempco_init() Error - timer init failed.\n");
        return status;
    }
    NSX_POWERTEMPCO_LOG("TempCo timer armed: period_us=%lu\n", (unsigned long)s_nsx_power_tempco_timer.periodInMicroseconds);

    status = am_hal_pwrctrl_tempco_init(state->adc_handle, state->adc_slot);
    if (status != AM_HAL_STATUS_SUCCESS) {
        nsx_printf("ERROR am_hal_pwrctrl_tempco_init() returned %d.\n", status);
        return status;
    }

    am_hal_adc_enable(state->adc_handle);

    state->initialized = true;
    state->wanted = true;
    state->callback_count = 0;
    NSX_POWERTEMPCO_LOG("TempCo enabled successfully\n");
    return NSX_STATUS_SUCCESS;
}

void nsx_power_tempco_prepare_for_sleep(void) {
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;

    if (!state->wanted || !state->initialized || state->adc_handle == NULL) {
        return;
    }

    (void)am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
}

static void nsx_power_tempco_callback(nsx_timer_config_t *cfg) {
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;

    (void)cfg;
    if (!state->initialized || state->adc_handle == NULL) {
        return;
    }

    (void)am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_WAKE, true);
    am_hal_adc_enable(state->adc_handle);

    if (nsx_power_tempco_capture_samples(state, AM_HAL_TEMPCO_NUMSAMPLES, state->samples) == AM_HAL_STATUS_SUCCESS) {
        state->last_handler_status =
            am_hal_pwrctrl_tempco_sample_handler(AM_HAL_TEMPCO_NUMSAMPLES, state->samples);
        state->callback_count++;
        NSX_POWERTEMPCO_LOG(
            "TempCo callback #%lu status=%lu sample0=0x%08lx\n",
            (unsigned long)state->callback_count,
            (unsigned long)state->last_handler_status,
            (unsigned long)state->samples[0].ui32Sample);
    }

    (void)am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
}

static void nsx_power_tempco_trigger_wait(nsx_power_tempco_state_t *state) {
    uint32_t fifo_count = _FLD2VAL(ADC_FIFO_COUNT, ADC->FIFO);

    while (_FLD2VAL(ADC_FIFO_COUNT, ADC->FIFO) == fifo_count) {
        am_hal_adc_sw_trigger(state->adc_handle);
        am_hal_delay_us(1);
    }
}

static uint32_t nsx_power_tempco_capture_samples(
    nsx_power_tempco_state_t *state,
    uint32_t num_samples,
    am_hal_adc_sample_t samples[]) {
    uint32_t sample_count = 1;
    uint32_t sample_idx = 0;

    for (uint32_t i = 0; i < AM_HAL_TEMPCO_NUMSAMPLES; ++i) {
        samples[i].ui32Sample = 0;
    }

    while (sample_idx < num_samples) {
        nsx_power_tempco_trigger_wait(state);
        am_hal_daxi_control(AM_HAL_DAXI_CONTROL_INVALIDATE, NULL);
        am_hal_adc_samples_read(state->adc_handle, true, NULL, &sample_count, &samples[sample_idx]);

        if (samples[sample_idx].ui32Slot == state->adc_slot) {
            sample_idx++;
        }
    }

    return AM_HAL_STATUS_SUCCESS;
}

#endif
