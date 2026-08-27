#if defined(ESP_PLATFORM)

#include "hal/hal_power.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#define PIN_BATTERY_ADC ADC_CHANNEL_3 // GPIO 4 on ESP32-S3 (ADC1 CH3)

static adc_oneshot_unit_handle_t s_adc_handle = NULL;

int hal_battery_init(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&init_config1, &s_adc_handle) != ESP_OK) {
        return -1;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc_handle, PIN_BATTERY_ADC, &config);
    return 0;
}

int hal_battery_read_mv(void) {
    if (!s_adc_handle) return -1;
    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, PIN_BATTERY_ADC, &raw) != ESP_OK) {
        return -1;
    }
    // T-Display S3 uses a 100k / 100k (2:1) voltage divider on GPIO 4:
    // Voltage = raw * 3300 * 2 / 4095
    return (raw * 6600) / 4095;
}

uint8_t hal_light_sensor_read(void) {
    return 100;
}

#endif
