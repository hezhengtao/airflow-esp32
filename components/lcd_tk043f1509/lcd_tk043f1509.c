#include "lcd_tk043f1509.h"
#include "LCD_TK040F1510.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "lcd";
static esp_lcd_panel_handle_t g_panel = NULL;

esp_err_t lcd_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "Init TK043F1509 LCD (NT35510 via TK040F1510 driver)");
    esp_err_t err;

    /* ── RD pin: set HIGH (not used, but must be driven) ────── */
    gpio_config_t rd_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LCD_RD_PIN),
    };
    gpio_config(&rd_cfg);
    gpio_set_level(LCD_RD_PIN, 1);

    /* ── Backlight (LEDC PWM) ────────────────────────────────── */
    if (LCD_BL_PIN >= 0) {
        ledc_timer_config_t bl_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_8_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = LCD_BL_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&bl_timer));

        ledc_channel_config_t bl_ch = {
            .gpio_num = LCD_BL_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&bl_ch));
    }

    /* ── 8080 parallel bus ───────────────────────────────────── */
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num = LCD_RS_PIN,
        .wr_gpio_num = LCD_WR_PIN,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            LCD_D0_PIN,  LCD_D1_PIN,  LCD_D2_PIN,  LCD_D3_PIN,
            LCD_D4_PIN,  LCD_D5_PIN,  LCD_D6_PIN,  LCD_D7_PIN,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
    };
    err = esp_lcd_new_i80_bus(&bus_cfg, &i80_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i80 bus create failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "i80 bus ready (D0-D7, RS=GPIO%d, WR=GPIO%d)", LCD_RS_PIN, LCD_WR_PIN);

    /* ── Panel IO ────────────────────────────────────────────── */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = 20000000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 16,
        .lcd_param_bits = 16,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
    };
    err = esp_lcd_new_panel_io_i80(i80_bus, &io_cfg, &io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO create failed: %s", esp_err_to_name(err));
        return err;
    }

    /* ── TK040F1510 panel driver (NT35510 init sequence) ─────── */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    esp_lcd_panel_handle_t panel = NULL;
    err = esp_lcd_new_panel_TK040F1510(io, &panel_cfg, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Panel create failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, false);
    /* Display + backlight off until boot screen renders (avoids snow flash) */
    lcd_set_backlight(0);

    *ret_io = io;
    *ret_panel = panel;
    g_panel = panel;

    ESP_LOGI(TAG, "LCD initialized (%dx%d)", LCD_WIDTH, LCD_HEIGHT);
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
