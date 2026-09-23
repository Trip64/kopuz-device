#include "hal/hal_ble_audio.h"
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
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define TAG "KOPUZ_BT"
#define BT_RINGBUF_SIZE (12 * 1024)

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
static uint64_t s_resample_accumulator = 0;
static bt_audio_state_t s_bt_state = BT_STATE_IDLE;
static char s_connected_device_name[64] = "Searching...";
static esp_bd_addr_t s_peer_bda;
static bool s_bt_inited = false;
static bool s_profile_ready = false;
static bool s_connect_pending = false;

static bt_device_entry_t s_discovered[8];
static uint8_t s_discovered_count = 0;
static bool s_is_scanning = false;
/* GAP callbacks run on a different task from the UI and serial console. */
static portMUX_TYPE s_discovered_lock = portMUX_INITIALIZER_UNLOCKED;

/* Caller holds s_discovered_lock. */
static int find_discovered_locked(const esp_bd_addr_t bda) {
    for (uint8_t index = 0; index < s_discovered_count; ++index) {
        if (memcmp(s_discovered[index].bda, bda, sizeof(esp_bd_addr_t)) == 0) {
            return index;
        }
    }
    return -1;
}

static void copy_device_name(char *destination, size_t destination_size,
                             const uint8_t *name, size_t name_length) {
    if (!destination || destination_size == 0 || !name || name_length == 0) return;
    if (name_length >= destination_size) name_length = destination_size - 1;
    memcpy(destination, name, name_length);
    destination[name_length] = '\0';
}

static void remember_discovered(const esp_bd_addr_t bda, const uint8_t *name,
                                size_t name_length, int8_t rssi) {
    char logged_name[sizeof(s_discovered[0].name)];
    portENTER_CRITICAL(&s_discovered_lock);
    int index = find_discovered_locked(bda);
    if (index < 0) {
        if (s_discovered_count >= 8) {
            portEXIT_CRITICAL(&s_discovered_lock);
            return;
        }
        index = s_discovered_count++;
        memset(&s_discovered[index], 0, sizeof(s_discovered[index]));
        memcpy(s_discovered[index].bda, bda, sizeof(esp_bd_addr_t));
        snprintf(s_discovered[index].name, sizeof(s_discovered[index].name),
                 "BT device %u", (unsigned)index + 1);
    }
    if (name && name_length > 0) {
        copy_device_name(s_discovered[index].name, sizeof(s_discovered[index].name),
                         name, name_length);
    }
    s_discovered[index].rssi = rssi;
    snprintf(logged_name, sizeof(logged_name), "%s", s_discovered[index].name);
    portEXIT_CRITICAL(&s_discovered_lock);
    ESP_LOGI(TAG, "Discovery result [%u]: %s (%d dBm)",
             (unsigned)index + 1, logged_name, rssi);
}

// Pull PCM data from ringbuffer into Bluetooth A2DP stream
static int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len) {
    if (!data || len <= 0 || !s_ble_ringbuf) {
        return 0;
    }

    size_t copied = 0;
    while (copied < (size_t)len) {
        size_t item_size = 0;
        /* A byte-buffer read may stop at the ring's wrap point. Read the
         * remaining segment too, without blocking the Bluetooth callback. */
        uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(
            s_ble_ringbuf, &item_size, 0, (size_t)len - copied);
        if (!item || item_size == 0) break;
        memcpy(data + copied, item, item_size);
        copied += item_size;
        vRingbufferReturnItem(s_ble_ringbuf, item);
    }

    /* Underrun: fill only the missing tail with silence. */
    if (copied < (size_t)len) memset(data + copied, 0, (size_t)len - copied);
    return len;
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT: {
            const uint8_t *device_name = NULL;
            size_t device_name_length = 0;
            int8_t rssi = -127;
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                    device_name = (const uint8_t *)param->disc_res.prop[i].val;
                    device_name_length = (size_t)param->disc_res.prop[i].len;
                } else if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_RSSI &&
                           param->disc_res.prop[i].len >= (int)sizeof(int8_t)) {
                    rssi = *(const int8_t *)param->disc_res.prop[i].val;
                } else if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR &&
                           !device_name) {
                    uint8_t *eir = (uint8_t*)param->disc_res.prop[i].val;
                    uint8_t rlen = 0;
                    uint8_t *name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rlen);
                    if (!name) {
                        name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rlen);
                    }
                    if (name && rlen > 0) {
                        device_name = name;
                        device_name_length = rlen;
                    }
                }
            }
            /* Some audio devices report BDNAME, while others omit a name entirely. */
            remember_discovered(param->disc_res.bda, device_name,
                                device_name_length, rssi);
            break;
        }
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
                portENTER_CRITICAL(&s_discovered_lock);
                int index = find_discovered_locked(param->read_rmt_name.bda);
                if (index >= 0) {
                    copy_device_name(s_discovered[index].name,
                                     sizeof(s_discovered[index].name),
                                     param->read_rmt_name.rmt_name,
                                     strnlen((const char *)param->read_rmt_name.rmt_name,
                                             ESP_BT_GAP_MAX_BDNAME_LEN));
                }
                portEXIT_CRITICAL(&s_discovered_lock);
            }
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                s_is_scanning = false;
                esp_bd_addr_t unnamed_bda;
                bool has_unnamed = false;
                portENTER_CRITICAL(&s_discovered_lock);
                for (uint8_t index = 0; index < s_discovered_count; ++index) {
                    if (!strncmp(s_discovered[index].name, "BT device ", 10)) {
                        memcpy(unnamed_bda, s_discovered[index].bda, sizeof(unnamed_bda));
                        has_unnamed = true;
                        break;
                    }
                }
                portEXIT_CRITICAL(&s_discovered_lock);
                if (has_unnamed) esp_bt_gap_read_remote_name(unnamed_bda);
                if (s_connect_pending) {
                    s_connect_pending = false;
                    esp_err_t error = esp_a2d_source_connect(s_peer_bda);
                    if (error != ESP_OK) {
                        s_bt_state = BT_STATE_IDLE;
                        ESP_LOGE(TAG, "A2DP connection start failed: %s",
                                 esp_err_to_name(error));
                    }
                }
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                s_is_scanning = true;
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT: {
            esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
            uint8_t length = 4;
            if (param->pin_req.min_16_digit) {
                memset(pin_code, 0, sizeof(pin_code));
                length = 16;
            }
            esp_bt_gap_pin_reply(param->pin_req.bda, true, length, pin_code);
            break;
        }
        case ESP_BT_GAP_CFM_REQ_EVT:
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        default:
            break;
    }
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_PROF_STATE_EVT:
            s_profile_ready = param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS;
            ESP_LOGI(TAG, "A2DP source profile %s",
                     s_profile_ready ? "ready" : "stopped");
            break;
        case ESP_A2D_CONNECTION_STATE_EVT: {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "Bluetooth A2DP Connected!");
                s_bt_state = BT_STATE_CONNECTED;
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                portENTER_CRITICAL(&s_discovered_lock);
                for (uint8_t d = 0; d < s_discovered_count; d++) {
                    if (memcmp(s_discovered[d].bda, param->conn_stat.remote_bda, 6) == 0) {
                        s_discovered[d].connected = true;
                        strncpy(s_connected_device_name, s_discovered[d].name, sizeof(s_connected_device_name) - 1);
                    } else {
                        s_discovered[d].connected = false;
                    }
                }
                portEXIT_CRITICAL(&s_discovered_lock);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "Bluetooth A2DP Disconnected");
                s_bt_state = BT_STATE_IDLE;
                snprintf(s_connected_device_name, sizeof(s_connected_device_name), "Disconnected");
                portENTER_CRITICAL(&s_discovered_lock);
                for (uint8_t d = 0; d < s_discovered_count; d++) {
                    s_discovered[d].connected = false;
                }
                portEXIT_CRITICAL(&s_discovered_lock);
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
    uint32_t new_sample_rate = sample_rate ? sample_rate : 44100;
    uint8_t new_channels = channels == 1 ? 1 : 2;
    if (new_sample_rate != s_ble_sample_rate || new_channels != s_ble_channels) {
        s_resample_accumulator = 0;
    }
    s_ble_sample_rate = new_sample_rate;
    s_ble_channels = new_channels;

    if (!s_ble_ringbuf) {
        s_ble_ringbuf = xRingbufferCreate(BT_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (!s_ble_ringbuf) {
            ESP_LOGE(TAG, "Failed to allocate Bluetooth RingBuffer");
            return -1;
        }
    }

    if (!s_bt_inited) {
        esp_err_t error = nvs_flash_init();
        if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            error = nvs_flash_erase();
            if (error == ESP_OK) error = nvs_flash_init();
        }
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth NVS initialization failed: %s", esp_err_to_name(error));
            return -1;
        }

        error = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Could not release unused BLE memory: %s", esp_err_to_name(error));
        }
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        error = esp_bt_controller_init(&bt_cfg);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(error));
            return -1;
        }
        error = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(error));
            return -1;
        }
        error = esp_bluedroid_init();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(error));
            return -1;
        }
        error = esp_bluedroid_enable();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(error));
            return -1;
        }

        esp_bt_sp_param_t security_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t capability = ESP_BT_IO_CAP_NONE;
        esp_bt_gap_set_security_param(security_type, &capability, sizeof(uint8_t));

        if (esp_bt_gap_set_device_name("Kopuz by Trip64") != ESP_OK ||
            esp_bt_gap_register_callback(bt_app_gap_cb) != ESP_OK ||
            esp_a2d_register_callback(bt_app_a2d_cb) != ESP_OK ||
            esp_a2d_source_register_data_callback(bt_app_a2d_data_cb) != ESP_OK ||
            esp_a2d_source_init() != ESP_OK) {
            ESP_LOGE(TAG, "A2DP source setup failed");
            return -1;
        }

        s_bt_inited = true;
        s_bt_state = BT_STATE_IDLE;
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
        s_profile_ready = false;
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
    if (!hal_ble_audio_is_connected()) return sample_count;

    int16_t pcm16[256];
    size_t output_samples = 0;
    size_t input_frames = sample_count / s_ble_channels;

    for (size_t frame = 0; frame < input_frames; ++frame) {
        s_resample_accumulator += 44100;
        while (s_resample_accumulator >= s_ble_sample_rate) {
            int32_t left = samples[frame * s_ble_channels] >> 16;
            int32_t right = s_ble_channels == 2 ?
                            samples[frame * 2 + 1] >> 16 : left;
            left = (int32_t)(((int64_t)left * s_ble_volume) / 100);
            right = (int32_t)(((int64_t)right * s_ble_volume) / 100);
            if (left > 32767) left = 32767;
            if (left < -32768) left = -32768;
            if (right > 32767) right = 32767;
            if (right < -32768) right = -32768;
            pcm16[output_samples++] = (int16_t)left;
            pcm16[output_samples++] = (int16_t)right;
            s_resample_accumulator -= s_ble_sample_rate;

            if (output_samples == sizeof(pcm16) / sizeof(pcm16[0])) {
                if (xRingbufferSend(s_ble_ringbuf, pcm16, sizeof(pcm16),
                                    pdMS_TO_TICKS(20)) != pdTRUE) {
                    return frame * s_ble_channels;
                }
                output_samples = 0;
            }
        }
    }
    if (output_samples > 0 &&
        xRingbufferSend(s_ble_ringbuf, pcm16, output_samples * sizeof(int16_t),
                        pdMS_TO_TICKS(20)) != pdTRUE) {
        return 0;
    }
    return input_frames * s_ble_channels;
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
    if (!s_bt_inited) return;
    if (s_is_scanning) return;
    portENTER_CRITICAL(&s_discovered_lock);
    s_discovered_count = 0;
    portEXIT_CRITICAL(&s_discovered_lock);
    s_is_scanning = true;
    esp_err_t error = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    if (error != ESP_OK) {
        s_is_scanning = false;
        ESP_LOGE(TAG, "Bluetooth discovery failed: %s", esp_err_to_name(error));
    }
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
    portENTER_CRITICAL(&s_discovered_lock);
    uint8_t count = (s_discovered_count < max_count) ? s_discovered_count : max_count;
    memcpy(devices, s_discovered, count * sizeof(bt_device_entry_t));
    portEXIT_CRITICAL(&s_discovered_lock);
    return count;
}

bool hal_ble_audio_connect_device(uint8_t index) {
    if (!s_profile_ready) return false;
    portENTER_CRITICAL(&s_discovered_lock);
    if (index >= s_discovered_count) {
        portEXIT_CRITICAL(&s_discovered_lock);
        return false;
    }
    memcpy(s_peer_bda, s_discovered[index].bda, sizeof(esp_bd_addr_t));
    snprintf(s_connected_device_name, sizeof(s_connected_device_name), "%s",
             s_discovered[index].name);
    portEXIT_CRITICAL(&s_discovered_lock);
    s_bt_state = BT_STATE_CONNECTING;
    if (s_is_scanning) {
        s_connect_pending = true;
        if (esp_bt_gap_cancel_discovery() != ESP_OK) {
            s_connect_pending = false;
            s_bt_state = BT_STATE_IDLE;
            return false;
        }
        return true;
    }
    esp_err_t error = esp_a2d_source_connect(s_peer_bda);
    if (error != ESP_OK) s_bt_state = BT_STATE_IDLE;
    return error == ESP_OK;
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
