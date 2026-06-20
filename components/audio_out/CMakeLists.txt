idf_component_register(
    SRCS "audio_out.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2s esp_driver_gptimer hal soc esp_common esp_rom freertos
)
