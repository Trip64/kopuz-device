#include "hal/hal_power.h"

#if defined(PICO_BOARD) || defined(RASPBERRYPI_PICO) || defined(PICO_RP2040) || defined(PICO_RP2350)
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define PIN_BATTERY_ADC 26 // ADC0

int hal_battery_init(void) {
    adc_init();
    adc_gpio_init(PIN_BATTERY_ADC);
    return 0;
}

int hal_battery_read_mv(void) {
    adc_select_input(0);
    uint16_t raw = adc_read();
    // 3.3V reference, 12-bit ADC (0..4095), 2:1 resistor divider:
    // Voltage = raw * 3300 * 2 / 4095 = raw * 6600 / 4095
    return (int)(((uint32_t)raw * 6600) / 4095);
}

uint8_t hal_light_sensor_read(void) {
    return 100;
}

#endif
