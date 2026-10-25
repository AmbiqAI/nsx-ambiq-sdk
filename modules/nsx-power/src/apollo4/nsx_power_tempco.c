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
    volatile bool initialized;
    bool wanted;
    bool timer_attempted;
    bool power_attempted;
    bool enable_attempted;
    bool suspended;
    uint32_t cleanup_status;
    bool reset_required;
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

static uint32_t nsx_power_tempco_check_support(void) {
    uint32_t trim_rev = 0;
    uint32_t status = am_hal_mram_info_read(1, AM_REG_INFO1_TRIM_REV_O / 4, 1, &trim_rev);

    NSX_POWERTEMPCO_LOG("TempCo trim read: status=%lu trim_rev=%lu\n",
        (unsigned long)status,
        (unsigned long)trim_rev);

    if (status != AM_HAL_STATUS_SUCCESS) return status;
    return trim_rev != 0xFFFFFFFFu && trim_rev >= NSX_POWERTEMPCO_MIN_TRIM_REV
        ? NSX_STATUS_SUCCESS : NSX_STATUS_FAILURE;
}

/* Foreground only. Stop callbacks before touching their ADC resources. */
uint32_t nsx_power_tempco_disable(void) {
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;
    uint32_t status;
    state->initialized = state->wanted = false;
    if (state->reset_required) return state->cleanup_status;
#define CLEANUP(operation) do { \
    status = (operation); \
    if (status != NSX_STATUS_SUCCESS) { state->cleanup_status = status; return status; } \
} while (0)
    if (state->timer_attempted) {
        CLEANUP(nsx_timer_deinit(&s_nsx_power_tempco_timer));
        state->timer_attempted = false;
    }
    if (state->adc_handle != NULL) {
        if (state->suspended) {
            CLEANUP(am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_ADC));
            CLEANUP(am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_WAKE, true));
            state->suspended = false;
        }
        if (state->enable_attempted) {
            CLEANUP(am_hal_adc_disable(state->adc_handle));
            state->enable_attempted = false;
        }
        if (state->power_attempted) {
            /* ADC power_control discards the underlying power-disable status. */
            CLEANUP(am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_ADC));
            state->power_attempted = false;
        }
        status = am_hal_adc_deinitialize(state->adc_handle);
        if (status != NSX_STATUS_SUCCESS) {
            /* HAL may invalidate the handle even when deinitialize fails. */
            state->cleanup_status = status;
            state->reset_required = true;
            return status;
        }
        state->adc_handle = NULL;
    }
    state->cleanup_status = NSX_STATUS_SUCCESS;
    return NSX_STATUS_SUCCESS;
#undef CLEANUP
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
        return NSX_STATUS_SUCCESS;
    }
    status = nsx_power_tempco_disable();
    if (status != NSX_STATUS_SUCCESS) return status;
    status = nsx_power_tempco_check_support();
    if (status != NSX_STATUS_SUCCESS) return status;
    state->adc_slot = NSX_POWERTEMPCO_ADC_TEMPERATURE_SLOT;
    status = am_hal_adc_initialize(0, &state->adc_handle);
    if (status != NSX_STATUS_SUCCESS) {
        state->adc_handle = NULL;
        return status;
    }
#define SETUP(operation) do { \
    status = (operation); \
    if (status != NSX_STATUS_SUCCESS) goto failed; \
} while (0)
    state->power_attempted = true;
    SETUP(am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_ADC));
    SETUP(am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_WAKE, false));
    SETUP(am_hal_adc_configure(state->adc_handle, &adc_cfg));
    SETUP(am_hal_pwrctrl_tempco_init(state->adc_handle, state->adc_slot));
    state->enable_attempted = true;
    SETUP(am_hal_adc_enable(state->adc_handle));
    /* Arm last: early callbacks see initialized=false and cannot touch ADC. */
    state->timer_attempted = true;
    status = nsx_timer_init(&s_nsx_power_tempco_timer);
    if (status != NSX_STATUS_SUCCESS) {
        if (status == NSX_TIMER_STATUS_IN_USE) state->timer_attempted = false;
        goto failed;
    }
    state->callback_count = 0;
    state->last_handler_status = NSX_STATUS_SUCCESS;
    state->wanted = state->initialized = true;
    return NSX_STATUS_SUCCESS;
failed:
    (void)nsx_power_tempco_disable();
    return status;
#undef SETUP
}

void nsx_power_tempco_prepare_for_sleep(void) {
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;

    if (!state->wanted || !state->initialized || state->adc_handle == NULL) {
        return;
    }

    uint32_t sleep_status = am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
    if (sleep_status == AM_HAL_STATUS_SUCCESS) state->suspended = true;
    if (state->last_handler_status == AM_HAL_STATUS_SUCCESS) state->last_handler_status = sleep_status;
}

static void nsx_power_tempco_callback(nsx_timer_config_t *cfg) {
    nsx_power_tempco_state_t *state = &s_nsx_power_tempco;

    (void)cfg;
    if (!state->initialized || state->adc_handle == NULL) {
        return;
    }

    state->last_handler_status = am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_WAKE, state->suspended);
    if (state->last_handler_status != AM_HAL_STATUS_SUCCESS) return;
    state->suspended = false;
    state->last_handler_status = am_hal_adc_enable(state->adc_handle);
    if (state->last_handler_status != AM_HAL_STATUS_SUCCESS) return;

    state->last_handler_status = nsx_power_tempco_capture_samples(state, AM_HAL_TEMPCO_NUMSAMPLES, state->samples);
    if (state->last_handler_status == AM_HAL_STATUS_SUCCESS) {
        state->last_handler_status =
            am_hal_pwrctrl_tempco_sample_handler(AM_HAL_TEMPCO_NUMSAMPLES, state->samples);
        state->callback_count++;
        NSX_POWERTEMPCO_LOG(
            "TempCo callback #%lu status=%lu sample0=0x%08lx\n",
            (unsigned long)state->callback_count,
            (unsigned long)state->last_handler_status,
            (unsigned long)state->samples[0].ui32Sample);
    }

    uint32_t sleep_status = am_hal_adc_power_control(state->adc_handle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
    if (sleep_status == AM_HAL_STATUS_SUCCESS) state->suspended = true;
    if (state->last_handler_status == AM_HAL_STATUS_SUCCESS) state->last_handler_status = sleep_status;
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
