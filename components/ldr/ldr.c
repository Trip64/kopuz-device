#include "ldr.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "ldr";

#define LDR_ADC_UNIT    ADC_UNIT_2
#define LDR_ADC_CHANNEL ADC_CHANNEL_6
#define LDR_ADC_ATTEN   ADC_ATTEN_DB_12
#define LDR_SAMPLES     8

static adc_oneshot_unit_handle_t s_adc;
static bool s_inited;

int ldr_init(void) {
    if (s_inited) return 0;

    adc_oneshot_unit_init_cfg_t unit = {.unit_id = LDR_ADC_UNIT};
    esp_err_t e = adc_oneshot_new_unit(&unit, &s_adc);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit: %s", esp_err_to_name(e));
        return -1;
    }
    adc_oneshot_chan_cfg_t ch = {
        .atten = LDR_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    e = adc_oneshot_config_channel(s_adc, LDR_ADC_CHANNEL, &ch);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel: %s", esp_err_to_name(e));
        return -1;
    }

    s_inited = true;
    ESP_LOGI(TAG, "LDR ADC up (GPIO17 / ADC2_CH6)");
    return 0;
}

int ldr_read_raw(void) {
    if (!s_inited) return -1;

    int acc = 0, got = 0;
    for (int i = 0; i < LDR_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(s_adc, LDR_ADC_CHANNEL, &raw) != ESP_OK) continue;
        acc += raw;
        got++;
    }
    if (got == 0) return -1;
    return acc / got;
}
