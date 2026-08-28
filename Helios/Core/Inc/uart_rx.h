/*
 * SOLARIS - UART Data Receiver
 * Receives JSON frames from the Pi prediction engine
 */

#ifndef SOLARIS_UART_RX_H
#define SOLARIS_UART_RX_H

#include "stm32f7xx_hal.h"
#include <stdint.h>

/* Data frame from Pi engine */
typedef struct {
    /* Solar wind */
    float bz;           /* nT */
    float speed;        /* km/s */
    float density;      /* /cm³ */
    float pdyn;         /* nPa */

    /* Predictions */
    float dst;          /* nT, current */
    float dst_pred;     /* nT, 1h forecast */
    float sub_prob;     /* %, substorm probability */
    float kp;           /* Kp index estimate */

    /* Impact status (0=nominal, 1=minor, 2=moderate, 3=severe) */
    uint8_t hf_status;
    uint8_t gnss_status;
    uint8_t gic_status;

    /* Storm phase: 0=QUIET, 1=MAIN, 2=RECOVERY */
    uint8_t storm_phase;

    /* Sensor data (future: from LoRa nodes) */
    float mag_h;        /* nT, H component */
    float cosmic_rate;  /* counts/min */

    /* Ambient Local Sensors */
    float ambient_temp;
    float ambient_light;

    /* Frame validity */
    uint8_t valid;
    uint32_t last_rx_tick;
} SolarisFrame;

/* Initialize UART receiver */
void UART_RX_Init(UART_HandleTypeDef *huart);

/* Call from main loop - processes received bytes, returns 1 on new frame */
int UART_RX_Process(SolarisFrame *frame);

/* Get latest frame (may not be new) */
const SolarisFrame *UART_RX_GetFrame(void);

#endif /* SOLARIS_UART_RX_H */
