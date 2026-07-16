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
 #include "LCD_TK050F9039.h"
 #include "driver/gpio.h"
 #define TK050F9039_CMD_RAMCTRL               0x00  //no use
 #define TK050F9039_DATA_LITTLE_ENDIAN_BIT    (1 << 3)
 
 static const char *TAG = "lcd_panel.TK050F9039";
 
 static esp_err_t panel_TK050F9039_del(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK050F9039_reset(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK050F9039_init(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK050F9039_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data);
 static esp_err_t panel_TK050F9039_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
 static esp_err_t panel_TK050F9039_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
 static esp_err_t panel_TK050F9039_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
 static esp_err_t panel_TK050F9039_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
 static esp_err_t panel_TK050F9039_disp_on_off(esp_lcd_panel_t *panel, bool off);
 static esp_err_t panel_TK050F9039_sleep(esp_lcd_panel_t *panel, bool sleep);
 
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
 } TK050F9039_panel_t;
 
 esp_err_t
 esp_lcd_new_panel_TK050F9039(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                          esp_lcd_panel_handle_t *ret_panel)
 {
 #if CONFIG_LCD_ENABLE_DEBUG_LOG
     esp_log_level_set(TAG, ESP_LOG_DEBUG);
 #endif
     esp_err_t ret = ESP_OK;
     TK050F9039_panel_t *TK050F9039 = NULL;
     ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
     TK050F9039 = calloc(1, sizeof(TK050F9039_panel_t));
     ESP_GOTO_ON_FALSE(TK050F9039, ESP_ERR_NO_MEM, err, TAG, "no mem for TK050F9039 panel");
 
     if (panel_dev_config->reset_gpio_num >= 0) {
         gpio_config_t io_conf = {
             .mode = GPIO_MODE_OUTPUT,
             .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
         };
         ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
     }
 
     switch (panel_dev_config->rgb_endian) {
     case LCD_RGB_ENDIAN_RGB:
         TK050F9039->madctl_val = 0;
         break;
     case LCD_RGB_ENDIAN_BGR:
         TK050F9039->madctl_val |= LCD_CMD_BGR_BIT;
         break;
     default:
         ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
         break;
     }
 
     uint8_t fb_bits_per_pixel = 0;
     switch (panel_dev_config->bits_per_pixel) {
     case 16: // RGB565
         TK050F9039->colmod_val = 0x55;
         fb_bits_per_pixel = 16;
         break;
     case 18: // RGB666
         TK050F9039->colmod_val = 0x66;
         // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
         fb_bits_per_pixel = 24;
         break;
     default:
         ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
         break;
     }
 
     TK050F9039->ramctl_val_1 = 0x00;
     TK050F9039->ramctl_val_2 = 0xf0;    // Use big endian by default
     if ((panel_dev_config->data_endian) == LCD_RGB_DATA_ENDIAN_LITTLE) {
         // Use little endian
         TK050F9039->ramctl_val_2 |= TK050F9039_DATA_LITTLE_ENDIAN_BIT;
     }
 
     TK050F9039->io = io;
     TK050F9039->fb_bits_per_pixel = fb_bits_per_pixel;
     TK050F9039->reset_gpio_num = panel_dev_config->reset_gpio_num;
     TK050F9039->reset_level = panel_dev_config->flags.reset_active_high;
     TK050F9039->base.del = panel_TK050F9039_del;
     TK050F9039->base.reset = panel_TK050F9039_reset;
     TK050F9039->base.init = panel_TK050F9039_init;
     TK050F9039->base.draw_bitmap = panel_TK050F9039_draw_bitmap;
     TK050F9039->base.invert_color = panel_TK050F9039_invert_color;
     TK050F9039->base.set_gap = panel_TK050F9039_set_gap;
     TK050F9039->base.mirror = panel_TK050F9039_mirror;
     TK050F9039->base.swap_xy = panel_TK050F9039_swap_xy;
     TK050F9039->base.disp_on_off = panel_TK050F9039_disp_on_off;
     TK050F9039->base.disp_sleep = panel_TK050F9039_sleep;
     *ret_panel = &(TK050F9039->base);
     ESP_LOGD(TAG, "new TK050F9039 panel @%p", TK050F9039);
 
     return ESP_OK;
 
 err:
     if (TK050F9039) {
         if (panel_dev_config->reset_gpio_num >= 0) {
             gpio_reset_pin(panel_dev_config->reset_gpio_num);
         }
         free(TK050F9039);
     }
     return ret;
 }
 
 static esp_err_t panel_TK050F9039_del(esp_lcd_panel_t *panel)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
 
     if (TK050F9039->reset_gpio_num >= 0) {
         gpio_reset_pin(TK050F9039->reset_gpio_num);
     }
     ESP_LOGD(TAG, "del TK050F9039 panel @%p", TK050F9039);
     free(TK050F9039);
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_reset(esp_lcd_panel_t *panel)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
 
     // perform hardware reset
     if (TK050F9039->reset_gpio_num >= 0) {
         gpio_set_level(TK050F9039->reset_gpio_num, TK050F9039->reset_level);
         vTaskDelay(pdMS_TO_TICKS(10));
         gpio_set_level(TK050F9039->reset_gpio_num, !TK050F9039->reset_level);
         vTaskDelay(pdMS_TO_TICKS(10));
     } else { // perform software reset
         ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG,
                             "io tx param failed");
         vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
     }
 
     return ESP_OK;
 }
 
 
 static esp_err_t panel_TK050F9039_init(esp_lcd_panel_t *panel)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
     // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF000,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF001,(uint8_t[]) { 0xAA,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF002,(uint8_t[]) { 0x52,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF003,(uint8_t[]) { 0x08,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF004,(uint8_t[]) { 0x01,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD100,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD101,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD102,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD103,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD104,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD105,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD106,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD107,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD108,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD109,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD10F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD110,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD111,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD112,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD113,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD114,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD115,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD116,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD117,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD118,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD119,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD11F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD120,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD121,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD122,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD123,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD124,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD125,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD126,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD127,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD128,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD129,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12B,(uint8_t[]) { 0x4A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD12F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD130,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD131,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD132,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD133,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD134,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD200,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD201,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD202,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD203,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD204,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD205,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD206,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD207,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD208,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD209,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD20F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD210,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD211,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD212,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD213,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD214,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD215,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD216,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD217,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD218,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD219,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD21F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD220,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD221,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD222,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD223,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD224,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD225,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD226,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD227,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD228,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD229,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22B,(uint8_t[]) { 0x4a,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD22F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD230,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD231,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD232,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD233,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD234,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD300,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD301,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD302,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD303,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD304,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD305,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD306,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD307,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD308,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD309,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD30F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD310,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD311,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD312,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD313,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD314,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD315,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD316,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD317,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD318,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD319,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD31F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD320,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD321,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD322,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD323,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD324,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD325,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD326,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD327,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD328,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD329,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32B,(uint8_t[]) { 0x4A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD32F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD330,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD331,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD332,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD333,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD334,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD400,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD401,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD402,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD403,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD404,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD405,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD406,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD407,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD408,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD409,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD40F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD410,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD411,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD412,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD413,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD414,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD415,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD416,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD417,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD418,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD419,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD41F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD420,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD421,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD422,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD423,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD424,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD425,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD426,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD427,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD428,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD429,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42B,(uint8_t[]) { 0x4A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD42F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD430,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD431,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD432,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD433,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD434,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD500,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD501,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD502,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD503,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD504,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD505,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD506,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD507,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD508,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD509,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD50F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD510,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD511,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD512,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD513,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD514,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD515,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD516,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD517,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD518,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD519,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD51F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD520,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD521,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD522,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD523,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD524,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD525,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD526,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD527,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD528,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD529,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52B,(uint8_t[]) { 0x4a,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD52F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD530,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD531,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD532,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD533,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD534,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD600,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD601,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD602,(uint8_t[]) { 0x1b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD603,(uint8_t[]) { 0x44,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD604,(uint8_t[]) { 0x62,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD605,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD606,(uint8_t[]) { 0x7b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD607,(uint8_t[]) { 0xa1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD608,(uint8_t[]) { 0xc0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD609,(uint8_t[]) { 0xee,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60A,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60B,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60C,(uint8_t[]) { 0x2c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60D,(uint8_t[]) { 0x43,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60E,(uint8_t[]) { 0x57,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD60F,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD610,(uint8_t[]) { 0x68,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD611,(uint8_t[]) { 0x78,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD612,(uint8_t[]) { 0x87,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD613,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD614,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD615,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD616,(uint8_t[]) { 0xac,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD617,(uint8_t[]) { 0xb6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD618,(uint8_t[]) { 0xc1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD619,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61A,(uint8_t[]) { 0xcb,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61B,(uint8_t[]) { 0xcd,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61C,(uint8_t[]) { 0xd6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61D,(uint8_t[]) { 0xdf,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61E,(uint8_t[]) { 0x95,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD61F,(uint8_t[]) { 0xe8,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD620,(uint8_t[]) { 0xf1,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD621,(uint8_t[]) { 0xfa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD622,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD623,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD624,(uint8_t[]) { 0x0b,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD625,(uint8_t[]) { 0x13,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD626,(uint8_t[]) { 0x1d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD627,(uint8_t[]) { 0x26,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD628,(uint8_t[]) { 0xaa,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD629,(uint8_t[]) { 0x30,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62A,(uint8_t[]) { 0x3c,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62B,(uint8_t[]) { 0x4A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62C,(uint8_t[]) { 0x63,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62D,(uint8_t[]) { 0xea,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62E,(uint8_t[]) { 0x79,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD62F,(uint8_t[]) { 0xa6,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD630,(uint8_t[]) { 0xd0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD631,(uint8_t[]) { 0x20,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD632,(uint8_t[]) { 0x0f,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD633,(uint8_t[]) { 0x8e,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xD634,(uint8_t[]) { 0xff,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB000,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB001,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB002,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB100,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB101,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB102,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB600,(uint8_t[]) { 0x34,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB601,(uint8_t[]) { 0x34,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB603,(uint8_t[]) { 0x34,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB700,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB701,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB702,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB800,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB801,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB802,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBA00,(uint8_t[]) { 0x14,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBA01,(uint8_t[]) { 0x14,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBA02,(uint8_t[]) { 0x14,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB900,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB901,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB902,(uint8_t[]) { 0x24,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBc00,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBc01,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBc02,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBd00,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBd01,(uint8_t[]) { 0xa0,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBd02,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBe01,(uint8_t[]) { 0x3d,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF000,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF001,(uint8_t[]) { 0xAA,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF002,(uint8_t[]) { 0x52,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF003,(uint8_t[]) { 0x08,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF004,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB400,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBC00,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBC01,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBC02,(uint8_t[]) { 0x05,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB700,(uint8_t[]) { 0x22,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB701,(uint8_t[]) { 0x22,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xC80B,(uint8_t[]) { 0x2A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xC80C,(uint8_t[]) { 0x2A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xC80F,(uint8_t[]) { 0x2A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xC810,(uint8_t[]) { 0x2A,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd000,(uint8_t[]) { 0x01,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb300,(uint8_t[]) { 0x10,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBd02,(uint8_t[]) { 0x07,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBe02,(uint8_t[]) { 0x07,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBf02,(uint8_t[]) { 0x07,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF000,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF001,(uint8_t[]) { 0xAA,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF002,(uint8_t[]) { 0x52,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF003,(uint8_t[]) { 0x08,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xF004,(uint8_t[]) { 0x02,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xc301,(uint8_t[]) { 0xa9,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xfe01,(uint8_t[]) { 0x94,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf600,(uint8_t[]) { 0x60,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3500,(uint8_t[]) { 0x00,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3600,(uint8_t[]) { 0xA3,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3A00,(uint8_t[]) { 0x55,  }, 2), TAG, "io tx param failed");

ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X1100, NULL, 0), TAG,"io tx param failed");
vTaskDelay(pdMS_TO_TICKS(50));
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X2900, NULL, 0), TAG,"io tx param failed");
vTaskDelay(pdMS_TO_TICKS(10));
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
 
     x_start += TK050F9039->x_gap;
     x_end += TK050F9039->x_gap;
     y_start += TK050F9039->y_gap;
     y_end += TK050F9039->y_gap;
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A00 + 0, (uint8_t[]) {
         (x_start >> 8) & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A00 + 1, (uint8_t[]) {
         x_start & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A00 + 2, (uint8_t[]) {
         ((x_end - 1) >> 8) & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A00 + 3, (uint8_t[]) {
         (x_end - 1) & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B00 + 0, (uint8_t[]) {
         (y_start >> 8) & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B00 + 1, (uint8_t[]) {
         y_start & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B00 + 2, (uint8_t[]) {
         ((y_end - 1) >> 8) & 0xFF,
     }, 2), TAG, "io tx param failed");
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B00 + 3, (uint8_t[]) {
         (y_end - 1) & 0xFF,
     }, 2), TAG, "io tx param failed");
     // transfer frame buffer
     size_t len = (x_end - x_start) * (y_end - y_start) * TK050F9039->fb_bits_per_pixel / 8;
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR_RB424, color_data, len), TAG, "io tx color failed"); // Write frame memory
 
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
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
 
 static esp_err_t panel_TK050F9039_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
     if (mirror_x) {
         TK050F9039->madctl_val |= LCD_CMD_MX_BIT;
     } else {
         TK050F9039->madctl_val &= ~LCD_CMD_MX_BIT;
     }
     if (mirror_y) {
         TK050F9039->madctl_val |= LCD_CMD_MY_BIT;
     } else {
         TK050F9039->madctl_val &= ~LCD_CMD_MY_BIT;
     }
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
         TK050F9039->madctl_val
     }, 1), TAG, "io tx param failed");
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
     if (swap_axes) {
         TK050F9039->madctl_val |= LCD_CMD_MV_BIT;
     } else {
         TK050F9039->madctl_val &= ~LCD_CMD_MV_BIT;
     }
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
         TK050F9039->madctl_val
     }, 1), TAG, "io tx param failed");
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     TK050F9039->x_gap = x_gap;
     TK050F9039->y_gap = y_gap;
     return ESP_OK;
 }
 
 static esp_err_t panel_TK050F9039_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
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
 
 static esp_err_t panel_TK050F9039_sleep(esp_lcd_panel_t *panel, bool sleep)
 {
     TK050F9039_panel_t *TK050F9039 = __containerof(panel, TK050F9039_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK050F9039->io;
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
 