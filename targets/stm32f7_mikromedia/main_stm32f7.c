#if defined(STM32F746xx) || defined(TARGET_STM32F7) || defined(TARGET_STM32F7_MIKROMEDIA)

#include "stm32f7xx_hal.h"
#include "app.h"
#include "audio_player.h"
#include "library/library.h"
#include "ui.h"
#include "hal/hal_audio.h"
#include "hal/hal_display.h"
#include "hal/hal_input.h"
#include "hal/hal_storage.h"
#include "hal/hal_power.h"
#include "hal/hal_system.h"
#include <string.h>

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 432;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    HAL_PWREx_EnableOverDrive();
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    /* RGB Status LEDs: PG15 = Red, PB3 = Green, PB4 = Blue */
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* Turn ON Green & Blue status LEDs */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
    
    /* TFT Data Bus: PE8-PE15 (High Byte) */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* TFT Data Bus: PG0-PG7 (Low Byte) */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    
    /* TFT Control Pins on Port F (PF10..PF15) */
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

extern void hal_system_check_dfu(void);

int main(void) {
    hal_system_check_dfu();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    hal_display_init();
    hal_input_init();
    hal_battery_init();
    hal_storage_mount();

    static uint8_t s_framebuf[LCD_FRAME_BYTES_1BPP];
    framebuffer_t fb;
    fb_init(&fb, s_framebuf, LCD_WIDTH, LCD_HEIGHT);

    static app_state_t s_app;
    app_init(&s_app);
    s_app.battery_mv = 3850; // 3.85V battery simulation

    // Scan MicroSD card
    library_scan(STORAGE_MOUNT_POINT, &s_app);

    // Use Nord Blue theme (Bright text on Slate Blue background)
    s_app.theme_index = 5;
    hal_display_set_theme(THEMES[s_app.theme_index].fg, THEMES[s_app.theme_index].bg);

    audio_player_init(&s_app);

    // Initial render and display flush
    ui_render(&fb, &s_app);
    hal_display_flush(fb.buffer);
    hal_display_present();
    s_app.dirty = false;

    uint32_t last_tick = HAL_GetTick();

    while (1) {
        btn_event_t btn = hal_input_poll();
        while (btn != BTN_NONE) {
            hal_audio_beep(1400, 20); // Pleasant feedback chime in earphones
            app_command_t cmd = app_on_button(&s_app, btn);
            if (cmd != CMD_NONE) {
                audio_player_send_command(cmd);
            }
            btn = hal_input_poll();
        }

        uint32_t now = HAL_GetTick();
        if (now - last_tick >= 50) {
            last_tick = now;

            if (s_app.state == PLAYBACK_PLAYING) {
                s_app.position_ms += 50;
                if (s_app.position_ms >= (uint32_t)s_app.queue[s_app.current_index].duration_secs * 1000) {
                    s_app.position_ms = 0;
                    app_on_button(&s_app, BTN_NEXT);
                }
                audio_player_tick_vu();
                s_app.dirty = true;
            }
        }

        if (s_app.dirty) {
            ui_render(&fb, &s_app);
            hal_display_flush(fb.buffer);
            if (s_app.screen == SCREEN_NOW_PLAYING && s_app.art_valid && s_app.art_rgb565) {
                hal_display_blit_rgb565(UI_ART_X, UI_ART_Y, s_app.art_size, s_app.art_size, s_app.art_rgb565);
            }
            hal_display_present();
            s_app.dirty = false;
        }

        HAL_Delay(5);
    }

    return 0;
}

#endif
