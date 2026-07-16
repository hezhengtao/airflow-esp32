/*
 * lcd_tk043f1509_rgb.c — RGB 16-bit parallel interface for NT35510
 *
 * Uses esp_lcd_new_rgb_panel() to create a PSRAM-based frame buffer.
 * DMA from PSRAM → LCD via RGB parallel bus — NO GDMA link-list limit.
 *
 * IMPORTANT: IM0/IM1 must be set HIGH on the LCD FPC to select RGB mode.
 *   IM0 (FPC pin 3) → 3.3V
 *   IM1 (FPC pin 4) → 3.3V
 */
#include "lcd_tk043f1509_rgb.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "lcd_rgb";
static esp_lcd_panel_handle_t g_panel = NULL;

esp_err_t lcd_init_rgb(esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "Init TK043F1509 LCD (NT35510 RGB 16-bit mode)");
    esp_err_t err;

    /* ── Backlight ────────────────────────────────────────────── */
    if (LCD_BL_PIN >= 0) {
        ledc_timer_config_t bl_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = LCD_BL_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        err = ledc_timer_config(&bl_timer);
        if (err != ESP_OK) ESP_LOGE(TAG, "Backlight timer: %s", esp_err_to_name(err));

        ledc_channel_config_t bl_ch = {
            .gpio_num = LCD_BL_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0,
            .hpoint = 0,
        };
        err = ledc_channel_config(&bl_ch);
        if (err != ESP_OK) ESP_LOGE(TAG, "Backlight channel: %s", esp_err_to_name(err));
    }

    /* ── RGB panel ────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Creating RGB panel: 800×480, 16-bit, PCLK=25MHz, PSRAM fb");
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 2,
        .bounce_buffer_size_px = 800 * 10,  /* 10-line bounce buffer in SRAM */
        .hsync_gpio_num = LCD_RGB_HSYNC,
        .vsync_gpio_num = LCD_RGB_VSYNC,
        .de_gpio_num = LCD_RGB_DE,
        .pclk_gpio_num = LCD_RGB_PCLK,
        .disp_gpio_num = LCD_RGB_DISP,
        .data_gpio_nums = {
            LCD_RGB_D0,  LCD_RGB_D1,  LCD_RGB_D2,  LCD_RGB_D3,
            LCD_RGB_D4,  LCD_RGB_D5,  LCD_RGB_D6,  LCD_RGB_D7,
            LCD_RGB_D8,  LCD_RGB_D9,  LCD_RGB_D10, LCD_RGB_D11,
            LCD_RGB_D12, LCD_RGB_D13, LCD_RGB_D14, LCD_RGB_D15,
        },
        .timings = {
            .pclk_hz = 25 * 1000 * 1000,
            .h_res = 800,
            .v_res = 480,
            .hsync_pulse_width = 40,
            .hsync_back_porch = 40,
            .hsync_front_porch = 48,
            .vsync_pulse_width = 23,
            .vsync_back_porch = 32,
            .vsync_front_porch = 13,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    esp_lcd_panel_handle_t panel = NULL;
    err = esp_lcd_new_rgb_panel(&panel_conf, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB panel create failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Reset and init the panel (reset pin, then wait for panel to boot) */
    esp_lcd_panel_reset(panel);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_lcd_panel_init(panel);

    /* Turn on display */
    esp_lcd_panel_disp_on_off(panel, true);

    /* Backlight on */
    lcd_set_backlight(80);

    *ret_panel = panel;
    g_panel = panel;

    ESP_LOGI(TAG, "RGB LCD initialized: 800×480, 16-bit RGB565, PSRAM fb");
    return ESP_OK;
}

void lcd_display_on(void)
{
    if (g_panel) {
        esp_lcd_panel_disp_on_off(g_panel, true);
        ESP_LOGI(TAG, "Display turned ON");
    }
}

void lcd_set_backlight(uint8_t duty_percent)
{
    if (LCD_BL_PIN < 0) return;
    /* Floor at duty=25 (~10%) so 0% brightness stays visible */
    uint32_t duty = 25 + (duty_percent * 230) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}
