#include "battery.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "battery";

// Deneyap Kart 1A v2: battery voltage on GPIO9 (BAT_VOLT_PIN), which is
// ADC1 channel 8. The board halves VBAT through a 2:1 divider, so the real
// battery voltage is twice the pin voltage.
#define BAT_ADC_UNIT    ADC_UNIT_1
#define BAT_ADC_CHANNEL ADC_CHANNEL_8
#define BAT_ADC_ATTEN   ADC_ATTEN_DB_12
#define BAT_DIVIDER     2
#define BAT_SAMPLES     8

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_cali_ok;
static bool s_inited;

int battery_init(void) {
    if (s_inited) return 0;

    adc_oneshot_unit_init_cfg_t unit = {.unit_id = BAT_ADC_UNIT};
    esp_err_t e = adc_oneshot_new_unit(&unit, &s_adc);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit: %s", esp_err_to_name(e));
        return -1;
    }
    adc_oneshot_chan_cfg_t ch = {
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    e = adc_oneshot_config_channel(s_adc, BAT_ADC_CHANNEL, &ch);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel: %s", esp_err_to_name(e));
        return -1;
    }

    adc_cali_curve_fitting_config_t cal = {
        .unit_id = BAT_ADC_UNIT,
        .chan = BAT_ADC_CHANNEL,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_cali_ok = (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) == ESP_OK);

    s_inited = true;
    ESP_LOGI(TAG, "battery ADC up (GPIO9 / ADC1_CH8, cali=%d)", s_cali_ok);
    return 0;
}

int battery_read_mv(void) {
    if (!s_inited) return -1;

    int acc = 0, got = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(s_adc, BAT_ADC_CHANNEL, &raw) != ESP_OK) continue;
        int mv = raw;
        if (s_cali_ok) {
            int cmv;
            if (adc_cali_raw_to_voltage(s_cali, raw, &cmv) == ESP_OK) mv = cmv;
        }
        acc += mv;
        got++;
    }
    if (got == 0) return -1;
    return (acc / got) * BAT_DIVIDER;
}
