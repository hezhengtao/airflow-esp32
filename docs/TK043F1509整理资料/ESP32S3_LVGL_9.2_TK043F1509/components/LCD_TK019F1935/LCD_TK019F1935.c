/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "sdkconfig.h"

#if CONFIG_LCD_ENABLE_DEBUG_LOG
// The local log level must be defined before including esp_log.h
// Set the maximum log level for this source file
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "LCD_TK019F1935.h"
#include "driver/gpio.h"
#define TK019F1935_CMD_RAMCTRL               0x00  //no use
#define TK019F1935_DATA_LITTLE_ENDIAN_BIT    (1 << 3)

static const char *TAG = "lcd_panel.TK019F1935";

static esp_err_t panel_TK019F1935_del(esp_lcd_panel_t *panel);
static esp_err_t panel_TK019F1935_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_TK019F1935_init(esp_lcd_panel_t *panel);
static esp_err_t panel_TK019F1935_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data);
static esp_err_t panel_TK019F1935_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_TK019F1935_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_TK019F1935_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_TK019F1935_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_TK019F1935_disp_on_off(esp_lcd_panel_t *panel, bool off);
static esp_err_t panel_TK019F1935_sleep(esp_lcd_panel_t *panel, bool sleep);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;    // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val;    // save current value of LCD_CMD_COLMOD register
    uint8_t ramctl_val_1;
    uint8_t ramctl_val_2;
} TK019F1935_panel_t;

esp_err_t
esp_lcd_new_panel_TK019F1935(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                         esp_lcd_panel_handle_t *ret_panel)
{
#if CONFIG_LCD_ENABLE_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    esp_err_t ret = ESP_OK;
    TK019F1935_panel_t *TK019F1935 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    TK019F1935 = calloc(1, sizeof(TK019F1935_panel_t));
    ESP_GOTO_ON_FALSE(TK019F1935, ESP_ERR_NO_MEM, err, TAG, "no mem for TK019F1935 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        TK019F1935->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        TK019F1935->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    uint8_t fb_bits_per_pixel = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        TK019F1935->colmod_val = 0x55;
        fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        TK019F1935->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    TK019F1935->ramctl_val_1 = 0x00;
    TK019F1935->ramctl_val_2 = 0xf0;    // Use big endian by default
    if ((panel_dev_config->data_endian) == LCD_RGB_DATA_ENDIAN_LITTLE) {
        // Use little endian
        TK019F1935->ramctl_val_2 |= TK019F1935_DATA_LITTLE_ENDIAN_BIT;
    }

    TK019F1935->io = io;
    TK019F1935->fb_bits_per_pixel = fb_bits_per_pixel;
    TK019F1935->reset_gpio_num = panel_dev_config->reset_gpio_num;
    TK019F1935->reset_level = panel_dev_config->flags.reset_active_high;
    TK019F1935->base.del = panel_TK019F1935_del;
    TK019F1935->base.reset = panel_TK019F1935_reset;
    TK019F1935->base.init = panel_TK019F1935_init;
    TK019F1935->base.draw_bitmap = panel_TK019F1935_draw_bitmap;
    TK019F1935->base.invert_color = panel_TK019F1935_invert_color;
    TK019F1935->base.set_gap = panel_TK019F1935_set_gap;
    TK019F1935->base.mirror = panel_TK019F1935_mirror;
    TK019F1935->base.swap_xy = panel_TK019F1935_swap_xy;
    TK019F1935->base.disp_on_off = panel_TK019F1935_disp_on_off;
    TK019F1935->base.disp_sleep = panel_TK019F1935_sleep;
    *ret_panel = &(TK019F1935->base);
    ESP_LOGD(TAG, "new TK019F1935 panel @%p", TK019F1935);

    return ESP_OK;

err:
    if (TK019F1935) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(TK019F1935);
    }
    return ret;
}

static esp_err_t panel_TK019F1935_del(esp_lcd_panel_t *panel)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);

    if (TK019F1935->reset_gpio_num >= 0) {
        gpio_reset_pin(TK019F1935->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del TK019F1935 panel @%p", TK019F1935);
    free(TK019F1935);
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_reset(esp_lcd_panel_t *panel)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;

    // perform hardware reset
    if (TK019F1935->reset_gpio_num >= 0) {
        gpio_set_level(TK019F1935->reset_gpio_num, TK019F1935->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(TK019F1935->reset_gpio_num, !TK019F1935->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG,
                            "io tx param failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
    }

    return ESP_OK;
}


static esp_err_t panel_TK019F1935_init(esp_lcd_panel_t *panel)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X11, NULL, 0), TAG,"io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]) { 0x05,0x2f,0x08,0x08,0x00 }, 10), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]) { 0x05 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) { 0x60 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X21, NULL, 0), TAG,"io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X29, NULL, 0), TAG,"io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]) { 0x55 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) { 0xA0 }, 2), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = TK019F1935->io;

    x_start += TK019F1935->x_gap;
    x_end += TK019F1935->x_gap;
    y_start += TK019F1935->y_gap;
    y_end += TK019F1935->y_gap;


    // define an area of frame memory where MCU can access

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 8), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 8), TAG, "io tx param failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * TK019F1935->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "io tx color failed"); // Write frame memory

    return ESP_OK;
}

static esp_err_t panel_TK019F1935_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG,
                        "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;
    if (mirror_x) {
        TK019F1935->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        TK019F1935->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        TK019F1935->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        TK019F1935->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        TK019F1935->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;
    if (swap_axes) {
        TK019F1935->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        TK019F1935->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        TK019F1935->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    TK019F1935->x_gap = x_gap;
    TK019F1935->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;
    int command = 0;
    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG,
                        "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK019F1935_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    TK019F1935_panel_t *TK019F1935 = __containerof(panel, TK019F1935_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK019F1935->io;
    int command = 0;
    if (sleep) {
        command = LCD_CMD_SLPIN;
    } else {
        command = LCD_CMD_SLPOUT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG,
                        "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}
