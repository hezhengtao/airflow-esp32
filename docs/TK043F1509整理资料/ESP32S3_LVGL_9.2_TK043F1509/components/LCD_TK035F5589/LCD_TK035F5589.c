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
#include "LCD_TK035F5589.h"
#include "driver/gpio.h"
#define TK035F5589_CMD_RAMCTRL               0x00  //no use
#define TK035F5589_DATA_LITTLE_ENDIAN_BIT    (1 << 3)

static const char *TAG = "lcd_panel.TK035F5589";

static esp_err_t panel_TK035F5589_del(esp_lcd_panel_t *panel);
static esp_err_t panel_TK035F5589_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_TK035F5589_init(esp_lcd_panel_t *panel);
static esp_err_t panel_TK035F5589_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data);
static esp_err_t panel_TK035F5589_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_TK035F5589_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_TK035F5589_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_TK035F5589_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_TK035F5589_disp_on_off(esp_lcd_panel_t *panel, bool off);
static esp_err_t panel_TK035F5589_sleep(esp_lcd_panel_t *panel, bool sleep);

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
} TK035F5589_panel_t;

esp_err_t
esp_lcd_new_panel_TK035F5589(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                         esp_lcd_panel_handle_t *ret_panel)
{
#if CONFIG_LCD_ENABLE_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    esp_err_t ret = ESP_OK;
    TK035F5589_panel_t *TK035F5589 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    TK035F5589 = calloc(1, sizeof(TK035F5589_panel_t));
    ESP_GOTO_ON_FALSE(TK035F5589, ESP_ERR_NO_MEM, err, TAG, "no mem for TK035F5589 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        TK035F5589->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        TK035F5589->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    uint8_t fb_bits_per_pixel = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        TK035F5589->colmod_val = 0x55;
        fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        TK035F5589->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    TK035F5589->ramctl_val_1 = 0x00;
    TK035F5589->ramctl_val_2 = 0xf0;    // Use big endian by default
    if ((panel_dev_config->data_endian) == LCD_RGB_DATA_ENDIAN_LITTLE) {
        // Use little endian
        TK035F5589->ramctl_val_2 |= TK035F5589_DATA_LITTLE_ENDIAN_BIT;
    }

    TK035F5589->io = io;
    TK035F5589->fb_bits_per_pixel = fb_bits_per_pixel;
    TK035F5589->reset_gpio_num = panel_dev_config->reset_gpio_num;
    TK035F5589->reset_level = panel_dev_config->flags.reset_active_high;
    TK035F5589->base.del = panel_TK035F5589_del;
    TK035F5589->base.reset = panel_TK035F5589_reset;
    TK035F5589->base.init = panel_TK035F5589_init;
    TK035F5589->base.draw_bitmap = panel_TK035F5589_draw_bitmap;
    TK035F5589->base.invert_color = panel_TK035F5589_invert_color;
    TK035F5589->base.set_gap = panel_TK035F5589_set_gap;
    TK035F5589->base.mirror = panel_TK035F5589_mirror;
    TK035F5589->base.swap_xy = panel_TK035F5589_swap_xy;
    TK035F5589->base.disp_on_off = panel_TK035F5589_disp_on_off;
    TK035F5589->base.disp_sleep = panel_TK035F5589_sleep;
    *ret_panel = &(TK035F5589->base);
    ESP_LOGD(TAG, "new TK035F5589 panel @%p", TK035F5589);

    return ESP_OK;

err:
    if (TK035F5589) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(TK035F5589);
    }
    return ret;
}

static esp_err_t panel_TK035F5589_del(esp_lcd_panel_t *panel)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);

    if (TK035F5589->reset_gpio_num >= 0) {
        gpio_reset_pin(TK035F5589->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del TK035F5589 panel @%p", TK035F5589);
    free(TK035F5589);
    return ESP_OK;
}

static esp_err_t panel_TK035F5589_reset(esp_lcd_panel_t *panel)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;

    // perform hardware reset
    if (TK035F5589->reset_gpio_num >= 0) {
        gpio_set_level(TK035F5589->reset_gpio_num, TK035F5589->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(TK035F5589->reset_gpio_num, !TK035F5589->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG,
                            "io tx param failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
    }

    return ESP_OK;
}


static esp_err_t panel_TK035F5589_init(esp_lcd_panel_t *panel)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;

    const uint8_t ga[24] ={0x06,0x0c,0x16,0x24,0x30,0x48,0x3d,0x28,0x20,0x14,0x0c,0x04,0x06,0x0c,0x16,0x24,0x30,0x48,0x3d,0x28,0x20,0x14,0x0c,0x04};

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X01, NULL, 0), TAG,"io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB0, (uint8_t[]) { 0x04 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB4, (uint8_t[]) { 0x00 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]) { 0x03,0xDF,0x40,0x12,0x00,0x01,0x00,0x43 }, 16), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]) { 0x05,0x2f,0x08,0x08,0x00 }, 10), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]) { 0x63,0x00,0x08,0x08 }, 8), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC8, ga, 48), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC9, ga, 48), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xCA, ga, 48), TAG, "io tx param failed");
  
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]) { 0x95,0x06,0x08,0x10,0x3F }, 10), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xD1, (uint8_t[]) { 0x02,0x28,0x28,0x40 }, 8), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE1, (uint8_t[]) { 0x00,0x00,0x00,0x00,0x00,0x00 }, 12), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE2, (uint8_t[]) { 0x80 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X11, NULL, 0), TAG,"io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X29, NULL, 0), TAG,"io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    //ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (uint16_t[]) { 0x05,0x2f,0x08,0x08,0x00 }, 10), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]) { 0x55 }, 2), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) { 0x60 }, 2), TAG, "io tx param failed");
    
    return ESP_OK;
}

static esp_err_t panel_TK035F5589_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                          const void *color_data)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = TK035F5589->io;

    x_start += TK035F5589->x_gap;
    x_end += TK035F5589->x_gap;
    y_start += TK035F5589->y_gap;
    y_end += TK035F5589->y_gap;


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
    size_t len = (x_end - x_start) * (y_end - y_start) * TK035F5589->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "io tx color failed"); // Write frame memory

    return ESP_OK;
}

static esp_err_t panel_TK035F5589_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;
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

static esp_err_t panel_TK035F5589_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;
    if (mirror_x) {
        TK035F5589->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        TK035F5589->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        TK035F5589->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        TK035F5589->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        TK035F5589->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK035F5589_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;
    if (swap_axes) {
        TK035F5589->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        TK035F5589->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        TK035F5589->madctl_val
    }, 1), TAG, "io tx param failed");
    return ESP_OK;
}

static esp_err_t panel_TK035F5589_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    TK035F5589->x_gap = x_gap;
    TK035F5589->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_TK035F5589_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;
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

static esp_err_t panel_TK035F5589_sleep(esp_lcd_panel_t *panel, bool sleep)
{
    TK035F5589_panel_t *TK035F5589 = __containerof(panel, TK035F5589_panel_t, base);
    esp_lcd_panel_io_handle_t io = TK035F5589->io;
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
