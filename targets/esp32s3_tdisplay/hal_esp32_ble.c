#include "hal_esp32_ble.h"
#include "hal/hal_audio.h"
#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM)

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define TAG "KOPUZ_BT"
#define BT_RINGBUF_SIZE (16 * 1024)

typedef enum {
    BT_STATE_IDLE = 0,
    BT_STATE_DISCOVERING,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
    BT_STATE_STREAMING
} bt_audio_state_t;

static RingbufHandle_t s_ble_ringbuf = NULL;
static uint8_t s_ble_volume = 70;
static uint32_t s_ble_sample_rate = 44100;
static uint8_t s_ble_channels = 2;
static bt_audio_state_t s_bt_state = BT_STATE_IDLE;
static char s_connected_device_name[64] = "Searching...";
static esp_bd_addr_t s_peer_bda;
static bool s_bt_inited = false;

static bt_device_entry_t s_discovered[8];
static uint8_t s_discovered_count = 0;
static bool s_is_scanning = false;

// Pull PCM data from ringbuffer into Bluetooth A2DP stream
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len) {
    if (!data || len <= 0 || !s_ble_ringbuf) {
        return 0;
    }

    size_t item_size = 0;
    uint8_t *item = (uint8_t*)xRingbufferReceiveUpTo(s_ble_ringbuf, &item_size, pdMS_TO_TICKS(10), (size_t)len);
    if (item && item_size > 0) {
        memcpy(data, item, item_size);
        vRingbufferReturnItem(s_ble_ringbuf, (void*)item);
        if ((int32_t)item_size < len) {
            memset(data + item_size, 0, (size_t)(len - (int32_t)item_size));
        }
        return len;
    }

    // Underrun: output silence to keep Bluetooth connection alive
    memset(data, 0, (size_t)len);
    return len;
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT: {
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR) {
                    uint8_t *eir = (uint8_t*)param->disc_res.prop[i].val;
                    uint8_t rlen = 0;
                    uint8_t *name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rlen);
                    if (!name) {
                        name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rlen);
                    }
                    if (name && rlen > 0) {
                        char dname[32];
                        size_t nlen = (rlen < sizeof(dname) - 1) ? rlen : sizeof(dname) - 1;
                        memcpy(dname, name, nlen);
                        dname[nlen] = '\0';

                        // Check if already in list
                        bool exists = false;
                        for (uint8_t d = 0; d < s_discovered_count; d++) {
                            if (memcmp(s_discovered[d].bda, param->disc_res.bda, 6) == 0) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists && s_discovered_count < 8) {
                            strncpy(s_discovered[s_discovered_count].name, dname, sizeof(s_discovered[0].name) - 1);
                            memcpy(s_discovered[s_discovered_count].bda, param->disc_res.bda, 6);
                            s_discovered[s_discovered_count].rssi = -55;
                            s_discovered[s_discovered_count].connected = false;
                            s_discovered_count++;
                            ESP_LOGI(TAG, "Discovered device [%u]: %s", s_discovered_count, dname);
                        }
                    }
                }
            }
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                s_is_scanning = false;
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                s_is_scanning = true;
            }
            break;
        }
        default:
            break;
    }
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "Bluetooth A2DP Connected!");
                s_bt_state = BT_STATE_CONNECTED;
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                for (uint8_t d = 0; d < s_discovered_count; d++) {
                    if (memcmp(s_discovered[d].bda, param->conn_stat.remote_bda, 6) == 0) {
                        s_discovered[d].connected = true;
                        strncpy(s_connected_device_name, s_discovered[d].name, sizeof(s_connected_device_name) - 1);
                    } else {
                        s_discovered[d].connected = false;
                    }
                }
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "Bluetooth A2DP Disconnected");
                s_bt_state = BT_STATE_IDLE;
                snprintf(s_connected_device_name, sizeof(s_connected_device_name), "Disconnected");
                for (uint8_t d = 0; d < s_discovered_count; d++) {
                    s_discovered[d].connected = false;
                }
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                s_bt_state = BT_STATE_STREAMING;
                ESP_LOGI(TAG, "Bluetooth Audio streaming active");
            }
            break;
        }
        default:
            break;
    }
}

int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels) {
    s_ble_sample_rate = sample_rate ? sample_rate : 44100;
    s_ble_channels = channels ? channels : 2;

    if (!s_ble_ringbuf) {
        s_ble_ringbuf = xRingbufferCreate(BT_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (!s_ble_ringbuf) {
            ESP_LOGE(TAG, "Failed to allocate Bluetooth RingBuffer");
            return -1;
        }
    }

    if (!s_bt_inited) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth controller init failed");
            return -1;
        }
        if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth controller enable failed");
            return -1;
        }
        if (esp_bluedroid_init() != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid init failed");
            return -1;
        }
        if (esp_bluedroid_enable() != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid enable failed");
            return -1;
        }

        esp_bt_dev_set_device_name("Kopuz Player");
        esp_bt_gap_register_callback(bt_app_gap_cb);
        esp_a2d_register_callback(bt_app_a2d_cb);
        esp_a2d_source_register_data_callback(bt_app_a2d_data_cb);
        esp_a2d_source_init();

        s_bt_inited = true;
        s_bt_state = BT_STATE_IDLE;
        hal_ble_audio_start_scan();
    }

    return 0;
}

void hal_ble_audio_deinit(void) {
    if (s_bt_inited) {
        esp_a2d_source_deinit();
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        s_bt_inited = false;
        s_bt_state = BT_STATE_IDLE;
    }
    if (s_ble_ringbuf) {
        vRingbufferDelete(s_ble_ringbuf);
        s_ble_ringbuf = NULL;
    }
    ESP_LOGI(TAG, "Bluetooth Audio deinitialized");
}

size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count) {
    if (!s_ble_ringbuf || !samples || sample_count == 0) return 0;

    int16_t pcm16[256];
    size_t done = 0;

    while (done < sample_count) {
        size_t chunk = sample_count - done;
        if (chunk > sizeof(pcm16) / sizeof(pcm16[0])) {
            chunk = sizeof(pcm16) / sizeof(pcm16[0]);
        }

        for (size_t i = 0; i < chunk; i++) {
            int32_t scaled = (int32_t)(((int64_t)(samples[done + i] >> 16) * s_ble_volume) / 100);
            if (scaled > 32767) scaled = 32767;
            if (scaled < -32768) scaled = -32768;
            pcm16[i] = (int16_t)scaled;
        }

        BaseType_t res = xRingbufferSend(s_ble_ringbuf, pcm16, chunk * sizeof(int16_t), pdMS_TO_TICKS(15));
        if (res != pdTRUE) {
            break;
        }
        done += chunk;
    }

    return done;
}

bool hal_ble_audio_is_connected(void) {
    return (s_bt_state == BT_STATE_CONNECTED || s_bt_state == BT_STATE_STREAMING);
}

void hal_ble_audio_set_volume(uint8_t vol) {
    s_ble_volume = (vol > 100) ? 100 : vol;
}

const char* hal_ble_audio_get_device_name(void) {
    return s_connected_device_name;
}

void hal_ble_audio_start_scan(void) {
    s_discovered_count = 0;
    s_is_scanning = true;
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

void hal_ble_audio_stop_scan(void) {
    s_is_scanning = false;
    esp_bt_gap_cancel_discovery();
}

bool hal_ble_audio_is_scanning(void) {
    return s_is_scanning;
}

uint8_t hal_ble_audio_get_discovered(bt_device_entry_t *devices, uint8_t max_count) {
    if (!devices || max_count == 0) return 0;
    uint8_t count = (s_discovered_count < max_count) ? s_discovered_count : max_count;
    memcpy(devices, s_discovered, count * sizeof(bt_device_entry_t));
    return count;
}

bool hal_ble_audio_connect_device(uint8_t index) {
    if (index >= s_discovered_count) return false;
    esp_bt_gap_cancel_discovery();
    s_is_scanning = false;
    memcpy(s_peer_bda, s_discovered[index].bda, sizeof(esp_bd_addr_t));
    s_bt_state = BT_STATE_CONNECTING;
    snprintf(s_connected_device_name, sizeof(s_connected_device_name), "%s", s_discovered[index].name);
    return (esp_a2d_source_connect(s_peer_bda) == ESP_OK);
}

void hal_ble_audio_disconnect(void) {
    if (s_bt_state == BT_STATE_CONNECTED || s_bt_state == BT_STATE_STREAMING) {
        esp_a2d_source_disconnect(s_peer_bda);
    }
}

#else

// Simulator and non-ESP platform fallback implementation
static bool s_sim_connected = false;
static uint8_t s_sim_volume = 70;
static bool s_sim_scanning = false;
static char s_sim_device_name[64] = "None";

static bt_device_entry_t s_sim_devices[4] = {
    { .name = "AirPods Pro",       .rssi = -42, .connected = false },
    { .name = "Sony WH-1000XM4",   .rssi = -55, .connected = false },
    { .name = "Galaxy Buds 2",     .rssi = -68, .connected = false },
    { .name = "JBL Flip 6",        .rssi = -74, .connected = false }
};

int hal_ble_audio_init(uint32_t sample_rate, uint8_t channels) {
    (void)sample_rate;
    (void)channels;
    return 0;
}

void hal_ble_audio_deinit(void) {
    s_sim_connected = false;
}

size_t hal_ble_audio_write(const int32_t *samples, size_t sample_count) {
    return hal_audio_write(samples, sample_count);
}

bool hal_ble_audio_is_connected(void) {
    return s_sim_connected;
}

void hal_ble_audio_set_volume(uint8_t vol) {
    s_sim_volume = vol;
}

const char* hal_ble_audio_get_device_name(void) {
    return s_sim_connected ? s_sim_device_name : (s_sim_scanning ? "Scanning..." : "Disconnected");
}

void hal_ble_audio_start_scan(void) {
    s_sim_scanning = true;
}

void hal_ble_audio_stop_scan(void) {
    s_sim_scanning = false;
}

bool hal_ble_audio_is_scanning(void) {
    return s_sim_scanning;
}

uint8_t hal_ble_audio_get_discovered(bt_device_entry_t *devices, uint8_t max_count) {
    if (!devices || max_count == 0) return 0;
    uint8_t count = (4 < max_count) ? 4 : max_count;
    memcpy(devices, s_sim_devices, count * sizeof(bt_device_entry_t));
    return count;
}

bool hal_ble_audio_connect_device(uint8_t index) {
    if (index >= 4) return false;
    for (int i = 0; i < 4; i++) {
        s_sim_devices[i].connected = (i == (int)index);
    }
    s_sim_connected = true;
    s_sim_scanning = false;
    snprintf(s_sim_device_name, sizeof(s_sim_device_name), "%s", s_sim_devices[index].name);
    return true;
}

void hal_ble_audio_disconnect(void) {
    for (int i = 0; i < 4; i++) {
        s_sim_devices[i].connected = false;
    }
    s_sim_connected = false;
    snprintf(s_sim_device_name, sizeof(s_sim_device_name), "Disconnected");
}

#endif
