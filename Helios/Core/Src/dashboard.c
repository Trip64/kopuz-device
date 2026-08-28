/*
 * SOLARIS - Premium Touch Dashboard Controller
 * Target: STM32F746 (480x272 SSD1963 TFT LCD)
 */

#include "dashboard.h"
#include "ssd1963.h"
#include "graphics.h"
#include "uart_rx.h"
#include "stmpe610.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ======================== PREMIUM RGB565 PALETTE ======================== */
#define C_BG            0x0841  /* Deep slate/blue */
#define C_PANEL         0x18E3  /* Lighter panel background */
#define C_BORDER        0x3186  /* Panel border */
#define C_TEXT          0xFFFF  /* White */
#define C_TEXT_DIM      0x7BEF  /* Gray/Silver */
#define C_CYAN          0x07FF  /* Primary accent */
#define C_BLUE          0x035F  /* Darker accent */
#define C_GREEN         0x07E0  /* Status OK */
#define C_YELLOW        0xFFE0  /* Status Warn */
#define C_RED           0xF800  /* Status Crit */

/* Layout Constants */
#define TAB_H       36
#define CHART_X     10
#define CHART_Y     (TAB_H + 10)
#define CHART_W     260
#define CHART_H     140

/* State Tracking */
static uint8_t view_mode = 0; /* 0 = MAIN, 1 = DIAGNOSTICS */
static uint8_t touch_found = 0;
static I2C_HandleTypeDef *h_i2c;
static uint32_t last_touch_time = 0;

static uint32_t rx_frames = 0;
static uint32_t boot_time = 0;

/* Chart History */
#define DST_HISTORY 240
static float dst_history[DST_HISTORY];
static float dst_pred_history[DST_HISTORY];
static uint16_t dst_idx = 0;
static uint8_t dst_wrapped = 0;
static uint8_t first_draw = 1;


/* ======================== HELPERS ======================== */
static uint16_t status_color(uint8_t level) {
    if (level == 0) return C_GREEN;
    if (level == 1) return C_YELLOW;
    if (level >= 2) return C_RED;
    return C_TEXT_DIM;
}

static const char *status_text(uint8_t level) {
    if (level == 0) return "OK";
    if (level == 1) return "WARN";
    if (level >= 2) return "CRIT";
    return "N/A";
}


/* ======================== TAB RENDERING ======================== */
static void DrawTabs(uint8_t active_tab)
{
    /* Main Tab */
    uint16_t m_color = (active_tab == 0) ? C_CYAN : C_BORDER;
    uint16_t m_bg    = (active_tab == 0) ? C_PANEL : C_BG;
    GFX_FillRect(0, 0, 240, TAB_H, m_bg);
    GFX_DrawRect(0, 0, 240, TAB_H, m_color);
    GFX_DrawString(60, 10, "TELEMETRY", C_TEXT, m_bg, 2);

    /* Diagnostics Tab */
    uint16_t d_color = (active_tab == 1) ? C_CYAN : C_BORDER;
    uint16_t d_bg    = (active_tab == 1) ? C_PANEL : C_BG;
    GFX_FillRect(240, 0, 240, TAB_H, d_bg);
    GFX_DrawRect(240, 0, 240, TAB_H, d_color);
    GFX_DrawString(290, 10, "DIAGNOSTICS", C_TEXT, d_bg, 2);
}


/* ======================== MAIN TAB (TELEMETRY) ======================== */
static void DrawChartBase(void)
{
    GFX_FillRect(CHART_X, CHART_Y, CHART_W, CHART_H, C_PANEL);
    GFX_DrawRect(CHART_X, CHART_Y, CHART_W, CHART_H, C_BORDER);
    GFX_DrawString(CHART_X + 6, CHART_Y + 6, "Dst PREDICTION (nT)", C_CYAN, C_PANEL, 1);

    int y_mid = CHART_Y + (CHART_H / 2);
    for (int dx = CHART_X + 2; dx < CHART_X + CHART_W - 2; dx += 6) {
        SSD1963_DrawPixel(dx, y_mid, C_BORDER); /* Zero line */
    }
}

static void UpdateChartData(const SolarisFrame *f)
{
    dst_history[dst_idx] = f->dst;
    dst_pred_history[dst_idx] = f->dst_pred;
    dst_idx = (dst_idx + 1) % DST_HISTORY;
    if (dst_idx == 0) dst_wrapped = 1;

    int chart_w = CHART_W - 4;
    int chart_h = CHART_H - 24;
    int y_off = CHART_Y + 20;

    GFX_FillRect(CHART_X + 2, y_off, chart_w, chart_h, C_PANEL);
    int y_mid = y_off + (chart_h / 2);
    for (int dx = CHART_X + 2; dx < CHART_X + CHART_W - 2; dx += 6) {
        SSD1963_DrawPixel(dx, y_mid, C_BORDER);
    }

    int count = dst_wrapped ? DST_HISTORY : dst_idx;
    if (count < 2) return;

    int start = dst_wrapped ? dst_idx : 0;
    int prev_x = 0, prev_y_d = 0, prev_y_p = 0;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % DST_HISTORY;
        int x = CHART_X + 2 + (i * chart_w) / count;
        
        float val = dst_history[idx];
        if (val > 100.0f) val = 100.0f;
        if (val < -300.0f) val = -300.0f;
        int y = y_off + (int)((100.0f - val) * chart_h / 400.0f);

        float pval = dst_pred_history[idx];
        if (pval > 100.0f) pval = 100.0f;
        if (pval < -300.0f) pval = -300.0f;
        int yp = y_off + (int)((100.0f - pval) * chart_h / 400.0f);

        if (i > 0) {
            GFX_DrawLine(prev_x, prev_y_p, x, yp, C_YELLOW); /* Predicted = Yellow */
            GFX_DrawLine(prev_x, prev_y_d, x, y, C_CYAN);  /* Actual = Cyan */
        }
        prev_x = x; prev_y_d = y; prev_y_p = yp;
    }
}

static void DrawMainPanels(const SolarisFrame *f)
{
    char buf[64];
    
    /* Right Side 1: Impacts (Y: 46) */
    int r_x = 280;
    int r_y = CHART_Y;
    GFX_FillRect(r_x, r_y, 190, 60, C_PANEL);
    GFX_DrawRect(r_x, r_y, 190, 60, C_BORDER);
    
    GFX_DrawString(r_x + 8, r_y + 8, "HF COMMS:", C_TEXT_DIM, C_PANEL, 1);
    GFX_DrawString(r_x + 90, r_y + 8, status_text(f->hf_status), status_color(f->hf_status), C_PANEL, 1);
    
    GFX_DrawString(r_x + 8, r_y + 24, "GPS/GNSS:", C_TEXT_DIM, C_PANEL, 1);
    GFX_DrawString(r_x + 90, r_y + 24, status_text(f->gnss_status), status_color(f->gnss_status), C_PANEL, 1);
    
    GFX_DrawString(r_x + 8, r_y + 40, "PWR GRID:", C_TEXT_DIM, C_PANEL, 1);
    GFX_DrawString(r_x + 90, r_y + 40, status_text(f->gic_status), status_color(f->gic_status), C_PANEL, 1);

    /* Right Side 2: Substorm Phase (Y: 110) */
    r_y += 65;
    GFX_FillRect(r_x, r_y, 190, 75, C_PANEL);
    GFX_DrawRect(r_x, r_y, 190, 75, C_BORDER);
    
    const char *phase_str = "PHASE: QUIET";
    uint16_t phase_col = C_GREEN;
    if (f->storm_phase == 1) { phase_str = "PHASE: MAIN"; phase_col = C_RED; }
    else if (f->storm_phase == 2) { phase_str = "PHASE: RECOV"; phase_col = C_YELLOW; }

    GFX_DrawString(r_x + 10, r_y + 10, phase_str, phase_col, C_PANEL, 2);
    
    snprintf(buf, sizeof(buf), "SUB PROB : %.0f%%", f->sub_prob);
    GFX_DrawString(r_x + 10, r_y + 45, buf, (f->sub_prob > 50) ? C_RED : C_CYAN, C_PANEL, 1);

    /* Bottom Sector (Solar Wind & Hardware) */
    int b_y = CHART_Y + CHART_H + 10;
    GFX_FillRect(10, b_y, 460, 65, C_PANEL);
    GFX_DrawRect(10, b_y, 460, 65, C_BORDER);
    
    /* Solar Wind Row */
    snprintf(buf, sizeof(buf), "SWBz: %+5.1f    V: %4.0f km/s   Kp: %2.0f", f->bz, f->speed, f->kp);
    GFX_DrawString(20, b_y + 10, buf, C_TEXT, C_PANEL, 2);

    /* GY-271 & Cosmic Ray Row */
    snprintf(buf, sizeof(buf), "GY-271 Mag Field: %.1f nT   CR: %.0f/s", f->mag_h, f->cosmic_rate);
    GFX_DrawString(20, b_y + 40, buf, C_CYAN, C_PANEL, 1);
}

/* ======================== DIAGNOSTICS TAB ======================== */
static void DrawDiagnosticsTab(const SolarisFrame *f, int force)
{
    if (force) {
        GFX_FillRect(0, TAB_H, TFT_WIDTH, TFT_HEIGHT - TAB_H, C_BG);
    }
    
    int cx = 20, cy = TAB_H + 20;
    
    GFX_DrawString(cx, cy, ">>> SOLARIS GATEWAY DIAGNOSTICS <<<", C_CYAN, C_BG, 2);
    cy += 40;

    char buf[64];
    /* Comms Link */
    uint16_t link_col = f->valid ? C_GREEN : C_RED;
    const char *link_str = f->valid ? "ESTABLISHED (ACTIVE)" : "DISCONNECTED / TIMEOUT";
    snprintf(buf, sizeof(buf), "PI UPLINK STATUS : %s", link_str);
    GFX_DrawString(cx, cy, buf, link_col, C_BG, 1);
    cy += 25;

    /* Metrics */
    snprintf(buf, sizeof(buf), "VALID JSON ACKs  : %lu FRAMES", rx_frames);
    GFX_DrawString(cx, cy, buf, C_TEXT, C_BG, 1);
    cy += 25;

    uint32_t uptime = (HAL_GetTick() - boot_time) / 1000;
    snprintf(buf, sizeof(buf), "SYSTEM UPTIME    : %lu SECONDS", uptime);
    GFX_DrawString(cx, cy, buf, C_TEXT, C_BG, 1);
    cy += 25;

    snprintf(buf, sizeof(buf), "F7 AMBIENT TEMP  : %.1f C", f->ambient_temp);
    GFX_DrawString(cx, cy, buf, C_YELLOW, C_BG, 1);
    cy += 25;

    snprintf(buf, sizeof(buf), "F7 AMBIENT LUX   : %.0f", f->ambient_light);
    GFX_DrawString(cx, cy, buf, C_YELLOW, C_BG, 1);
    cy += 40;

    /* Status Console Block */
    GFX_FillRect(cx, cy, 440, 80, C_PANEL);
    GFX_DrawRect(cx, cy, 440, 80, C_BORDER);
    GFX_DrawString(cx + 10, cy + 10, "> TERMINAL EVENT LOG", C_CYAN, C_PANEL, 1);
    
    if (f->valid) {
        GFX_DrawString(cx + 10, cy + 35, "> [OK] Checksum Verified.", C_GREEN, C_PANEL, 1);
        GFX_DrawString(cx + 10, cy + 55, "> [OK] ACK Transmitted Over UART6.", C_GREEN, C_PANEL, 1);
    } else {
        GFX_DrawString(cx + 10, cy + 35, "> [ERR] UART Dead Silence. Fallback Loaded.", C_RED, C_PANEL, 1);
        GFX_DrawString(cx + 10, cy + 55, "> [WAIT] Polling RX buffer for magic byte...", C_TEXT_DIM, C_PANEL, 1);
    }
}


/* ======================== PUBLIC API ======================== */
void Dashboard_Init(I2C_HandleTypeDef *hi2c)
{
    h_i2c = hi2c;
    boot_time = HAL_GetTick();

    memset(dst_history, 0, sizeof(dst_history));
    memset(dst_pred_history, 0, sizeof(dst_pred_history));
    dst_idx = 0;
    dst_wrapped = 0;
    first_draw = 1;
    view_mode = 0;

    if (HAL_I2C_IsDeviceReady(h_i2c, 0x41 << 1, 1, 100) == HAL_OK) {
        touch_found = STMPE610_Init(h_i2c, 0x41 << 1);
    } else if (HAL_I2C_IsDeviceReady(h_i2c, 0x44 << 1, 1, 100) == HAL_OK) {
        touch_found = STMPE610_Init(h_i2c, 0x44 << 1);
    } else {
        touch_found = STMPE610_Init(h_i2c, 0x41 << 1);
    }

    SSD1963_FillScreen(C_BG);
    DrawTabs(view_mode);
}

void Dashboard_Run(void)
{
    SolarisFrame frame = {0};
    uint32_t last_update = 0;
    uint8_t prev_view = 99; /* Force first render */

    while (1) {
        uint32_t now = HAL_GetTick();
        
        /* 1. TOUCH HANDLING - TAB NAVIGATION */
        if (touch_found && STMPE610_Touched()) {
            uint16_t x, y; uint8_t z;
            if (STMPE610_ReadXYZ(&x, &y, &z)) {
                if (now - last_touch_time > 300) {
                    /* Crude screen mapping for Mikroe */
                    int px = (x > 250) ? ((x - 250) * 480 / 3550) : 0;
                    int py = (y > 250) ? ((y - 250) * 272 / 3550) : 0;
                    
                    /* Did they tap the top UI layer (Tabs)? */
                    if (py < TAB_H + 20) {
                        if (px < 240 && view_mode != 0) {
                            view_mode = 0; /* Main Tab */
                        } else if (px >= 240 && view_mode != 1) {
                            view_mode = 1; /* Diags Tab */
                        }
                    }
                    last_touch_time = now;
                }
            }
        }
        
        /* 2. LOCAL HARDWARE POLLING */
        static uint32_t last_sensor_time = 0;
        if (now - last_sensor_time > 1000) {
            uint16_t adc6_temp = Read_ADC3_Channel(ADC_CHANNEL_6);
            uint16_t adc7_lux = Read_ADC3_Channel(ADC_CHANNEL_7);
            
            if (adc6_temp < 9000) {
                float mv = (2048.0f * adc6_temp) / 4096.0f;
                frame.ambient_temp = (mv - 500.0f) / 10.0f;
            }
            frame.ambient_light = (float)adc7_lux;
            last_sensor_time = now;
        }

        /* 3. COMMS POLLING */
        if (UART_RX_Process(&frame)) {
            rx_frames++; /* We received a valid JSON with verified Checksum! */
        }
        
        if (!frame.valid) {
            /* Zero out parameters if dead signal, but keep locals */
            frame.bz = 0.0f; frame.speed = 0.0f; frame.density = 0.0f;
            frame.kp = 0.0f; frame.hf_status = 0; frame.gnss_status = 0;
            frame.storm_phase = 0; frame.sub_prob = 0;
            frame.mag_h = 0.0f; frame.cosmic_rate = 0.0f;
        }

        /* 4. RENDER PIPELINE */
        int force_full = (view_mode != prev_view);

        if (force_full || (now - last_update > 200)) {
            if (force_full) {
                DrawTabs(view_mode);
            }

            if (view_mode == 0) {
                /* MAIN TELEMETRY TAB */
                if (force_full) {
                    GFX_FillRect(0, TAB_H, TFT_WIDTH, TFT_HEIGHT - TAB_H, C_BG);
                    DrawChartBase();
                }
                UpdateChartData(&frame);
                DrawMainPanels(&frame);
            } 
            else if (view_mode == 1) {
                /* DIAGNOSTICS TAB */
                DrawDiagnosticsTab(&frame, force_full);
            }
            
            last_update = now;
            prev_view = view_mode;
            first_draw = 0;
        }

        HAL_Delay(16); /* ~60fps target */
    }
}
