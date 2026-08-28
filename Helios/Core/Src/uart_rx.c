/*
 * SOLARIS - UART Data Receiver
 * Receives newline-delimited JSON from Pi, parses into SolarisFrame.
 *
 * Protocol: one JSON object per line, terminated by '\n'
 * Example: {"bz":-5.2,"v":600,"dst":-87,"hf":2,...}\n
 *
 * Uses interrupt-driven single-byte reception for reliability.
 */

#include "uart_rx.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define RX_BUF_SIZE 512

static UART_HandleTypeDef *h_uart;
static uint8_t rx_byte;
static char rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_pos = 0;
static volatile uint8_t frame_ready = 0;
static char parse_buf[RX_BUF_SIZE];
static SolarisFrame current_frame;

static uint8_t Calc_Checksum(const char *str) {
    uint8_t chk = 0;
    while (*str) {
        chk += (uint8_t)(*str);
        str++;
    }
    return chk;
}

/* Simple key-value JSON parser - no dependency, handles our known format */
static float json_get_float(const char *json, const char *key, float def)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char *p = strstr(json, search);
    if (!p) return def;
    p += strlen(search);

    /* Skip whitespace */
    while (*p == ' ') p++;

    return (float)strtod(p, NULL);
}

static int json_get_int(const char *json, const char *key, int def)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char *p = strstr(json, search);
    if (!p) return def;
    p += strlen(search);
    while (*p == ' ') p++;

    return (int)strtol(p, NULL, 10);
}

static uint8_t json_get_phase(const char *json)
{
    if (strstr(json, "\"MAIN\""))       return 1;
    if (strstr(json, "\"RECOVERY\""))   return 2;
    return 0; /* QUIET */
}

static void parse_frame(const char *json, SolarisFrame *f)
{
    f->bz       = json_get_float(json, "bz", 0.0f);
    f->speed    = json_get_float(json, "v", 400.0f);
    f->density  = json_get_float(json, "n", 5.0f);
    f->pdyn     = json_get_float(json, "pdyn", 2.0f);
    f->dst      = json_get_float(json, "dst", 0.0f);
    f->dst_pred = json_get_float(json, "dst_p", 0.0f);
    f->sub_prob = json_get_float(json, "sub_p", 0.0f);
    f->kp       = json_get_float(json, "kp", 0.0f);

    f->hf_status   = (uint8_t)json_get_int(json, "hf", 0);
    f->gnss_status = (uint8_t)json_get_int(json, "gnss", 0);
    f->gic_status  = (uint8_t)json_get_int(json, "gic", 0);

    f->storm_phase = json_get_phase(json);

    f->mag_h       = json_get_float(json, "mag_h", 0.0f);
    f->cosmic_rate = json_get_float(json, "cr", 0.0f);

    f->valid = 1;
    f->last_rx_tick = HAL_GetTick();
}

/* UART RX complete callback (called from IRQ) */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != h_uart) return;

    if (rx_byte == '\n') {
        if (rx_pos > 0 && rx_pos < RX_BUF_SIZE) {
            rx_buf[rx_pos] = '\0';
            memcpy(parse_buf, rx_buf, rx_pos + 1);
            frame_ready = 1;
        }
        rx_pos = 0;
    } else if (rx_pos < RX_BUF_SIZE - 1) {
        if (rx_byte != '\r') {
            rx_buf[rx_pos++] = rx_byte;
        }
    }

    /* Re-arm interrupt */
    HAL_UART_Receive_IT(h_uart, &rx_byte, 1);
}

void UART_RX_Init(UART_HandleTypeDef *huart)
{
    h_uart = huart;
    memset(&current_frame, 0, sizeof(current_frame));
    rx_pos = 0;
    frame_ready = 0;

    /* Start receiving */
    HAL_UART_Receive_IT(h_uart, &rx_byte, 1);
}

int UART_RX_Process(SolarisFrame *frame)
{
    float t = frame->ambient_temp;
    float l = frame->ambient_light;

    if (frame_ready) {
        frame_ready = 0;
        
        char *star = strrchr(parse_buf, '*');
        if (star) {
            *star = '\0'; /* Terminate string at '*' */
            uint8_t expected = Calc_Checksum(parse_buf);
            uint8_t received = (uint8_t)strtol(star + 1, NULL, 16);
            
            if (expected == received) {
                /* Valid payload -> Send ACK */
                const char *ack = "{\"type\":\"ack\"}";
                uint8_t ack_chk = Calc_Checksum(ack);
                char tx_buf[64];
                snprintf(tx_buf, sizeof(tx_buf), "%s*%02X\n", ack, ack_chk);
                HAL_UART_Transmit(h_uart, (uint8_t*)tx_buf, strlen(tx_buf), 100);
                
                parse_frame(parse_buf, &current_frame);
                *frame = current_frame;
                frame->ambient_temp = t;
                frame->ambient_light = l;
                return 1;
            } else {
                /* Invalid checksum -> Send Error */
                const char *err = "{\"type\":\"error\",\"source\":\"f7\",\"code\":\"CHECKSUM_FAIL\"}";
                uint8_t err_chk = Calc_Checksum(err);
                char tx_buf[128];
                snprintf(tx_buf, sizeof(tx_buf), "%s*%02X\n", err, err_chk);
                HAL_UART_Transmit(h_uart, (uint8_t*)tx_buf, strlen(tx_buf), 100);
            }
        }
        return 0;
    }

    /* Check for stale data (no frame in 5 seconds) */
    if (current_frame.valid &&
        (HAL_GetTick() - current_frame.last_rx_tick > 5000)) {
        current_frame.valid = 0;
    }

    *frame = current_frame;
    frame->ambient_temp = t;
    frame->ambient_light = l;
    return 0;
}

const SolarisFrame *UART_RX_GetFrame(void)
{
    return &current_frame;
}
