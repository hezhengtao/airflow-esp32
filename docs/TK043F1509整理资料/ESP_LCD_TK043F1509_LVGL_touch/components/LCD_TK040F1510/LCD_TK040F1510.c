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
 #include "LCD_TK040F1510.h"
 #include "driver/gpio.h"
 #define TK040F1510_CMD_RAMCTRL               0x00  //no use
 #define TK040F1510_DATA_LITTLE_ENDIAN_BIT    (1 << 3)
 
 static const char *TAG = "lcd_panel.TK040F1510";
 
 static esp_err_t panel_TK040F1510_del(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK040F1510_reset(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK040F1510_init(esp_lcd_panel_t *panel);
 static esp_err_t panel_TK040F1510_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data);
 static esp_err_t panel_TK040F1510_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
 static esp_err_t panel_TK040F1510_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
 static esp_err_t panel_TK040F1510_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
 static esp_err_t panel_TK040F1510_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
 static esp_err_t panel_TK040F1510_disp_on_off(esp_lcd_panel_t *panel, bool off);
 static esp_err_t panel_TK040F1510_sleep(esp_lcd_panel_t *panel, bool sleep);
 
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
 } TK040F1510_panel_t;
 
 esp_err_t
 esp_lcd_new_panel_TK040F1510(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                          esp_lcd_panel_handle_t *ret_panel)
 {
 #if CONFIG_LCD_ENABLE_DEBUG_LOG
     esp_log_level_set(TAG, ESP_LOG_DEBUG);
 #endif
     esp_err_t ret = ESP_OK;
     TK040F1510_panel_t *TK040F1510 = NULL;
     ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
     TK040F1510 = calloc(1, sizeof(TK040F1510_panel_t));
     ESP_GOTO_ON_FALSE(TK040F1510, ESP_ERR_NO_MEM, err, TAG, "no mem for TK040F1510 panel");
 
     if (panel_dev_config->reset_gpio_num >= 0) {
         gpio_config_t io_conf = {
             .mode = GPIO_MODE_OUTPUT,
             .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
         };
         ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
     }
 
     switch (panel_dev_config->rgb_endian) {
     case LCD_RGB_ENDIAN_RGB:
         TK040F1510->madctl_val = 0;
         break;
     case LCD_RGB_ENDIAN_BGR:
         TK040F1510->madctl_val |= LCD_CMD_BGR_BIT;
         break;
     default:
         ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
         break;
     }
 
     uint8_t fb_bits_per_pixel = 0;
     switch (panel_dev_config->bits_per_pixel) {
     case 16: // RGB565
         TK040F1510->colmod_val = 0x55;
         fb_bits_per_pixel = 16;
         break;
     case 18: // RGB666
         TK040F1510->colmod_val = 0x66;
         // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
         fb_bits_per_pixel = 24;
         break;
     default:
         ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
         break;
     }
 
     TK040F1510->ramctl_val_1 = 0x00;
     TK040F1510->ramctl_val_2 = 0xf0;    // Use big endian by default
     if ((panel_dev_config->data_endian) == LCD_RGB_DATA_ENDIAN_LITTLE) {
         // Use little endian
         TK040F1510->ramctl_val_2 |= TK040F1510_DATA_LITTLE_ENDIAN_BIT;
     }
 
     TK040F1510->io = io;
     TK040F1510->fb_bits_per_pixel = fb_bits_per_pixel;
     TK040F1510->reset_gpio_num = panel_dev_config->reset_gpio_num;
     TK040F1510->reset_level = panel_dev_config->flags.reset_active_high;
     TK040F1510->base.del = panel_TK040F1510_del;
     TK040F1510->base.reset = panel_TK040F1510_reset;
     TK040F1510->base.init = panel_TK040F1510_init;
     TK040F1510->base.draw_bitmap = panel_TK040F1510_draw_bitmap;
     TK040F1510->base.invert_color = panel_TK040F1510_invert_color;
     TK040F1510->base.set_gap = panel_TK040F1510_set_gap;
     TK040F1510->base.mirror = panel_TK040F1510_mirror;
     TK040F1510->base.swap_xy = panel_TK040F1510_swap_xy;
     TK040F1510->base.disp_on_off = panel_TK040F1510_disp_on_off;
     TK040F1510->base.disp_sleep = panel_TK040F1510_sleep;
     *ret_panel = &(TK040F1510->base);
     ESP_LOGD(TAG, "new TK040F1510 panel @%p", TK040F1510);
 
     return ESP_OK;
 
 err:
     if (TK040F1510) {
         if (panel_dev_config->reset_gpio_num >= 0) {
             gpio_reset_pin(panel_dev_config->reset_gpio_num);
         }
         free(TK040F1510);
     }
     return ret;
 }
 
 static esp_err_t panel_TK040F1510_del(esp_lcd_panel_t *panel)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
 
     if (TK040F1510->reset_gpio_num >= 0) {
         gpio_reset_pin(TK040F1510->reset_gpio_num);
     }
     ESP_LOGD(TAG, "del TK040F1510 panel @%p", TK040F1510);
     free(TK040F1510);
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_reset(esp_lcd_panel_t *panel)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
 
     // perform hardware reset
     if (TK040F1510->reset_gpio_num >= 0) {
         gpio_set_level(TK040F1510->reset_gpio_num, TK040F1510->reset_level);
         vTaskDelay(pdMS_TO_TICKS(10));
         gpio_set_level(TK040F1510->reset_gpio_num, !TK040F1510->reset_level);
         vTaskDelay(pdMS_TO_TICKS(10));
     } else { // perform software reset
         ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG,
                             "io tx param failed");
         vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5m before sending new command
     }
 
     return ESP_OK;
 }
 
//  static esp_err_t panel_TK040F1510_init(esp_lcd_panel_t *panel)
//  {
//      TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
//      esp_lcd_panel_io_handle_t io = TK040F1510->io;

//      // Sleep out
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0), 
//                       TAG, "Sleep out failed");
//     vTaskDelay(pdMS_TO_TICKS(100)); // Delay 100ms

//     //--------------------------------------Display Setting------------------------------------
//     // 65k color mode (16-bit)
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, 
//                       (uint8_t[]){0x05}, 1),
//                       TAG, "Color mode set failed");

//     // Memory data access control (orientation)
//     // 0x60: 横屏，从左下角开始，从左到右，从下到上
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, 
//                       (uint8_t[]){0x60}, 1),
//                       TAG, "Memory access control failed");

//     // Display inversion on (0x21) and display on (0x29)
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x21, NULL, 0),
//                       TAG, "Inversion on failed");
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0),
//                       TAG, "Display on failed");
//     vTaskDelay(pdMS_TO_TICKS(10));

//     // Alternative color mode and orientation settings
//     // 0x55: RGB565 (16-bit)
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, 
//                       (uint8_t[]){0x55}, 1),
//                       TAG, "RGB565 mode set failed");
    
//     // 0xA0: D3=1(RB swap), D5=1(横竖屏切换), D6/D7=00(镜像设置)
//     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, 
//                       (uint8_t[]){0xA0}, 1),
//                       TAG, "Advanced orientation set failed");
    
//     vTaskDelay(pdMS_TO_TICKS(20));

//          return ESP_OK;
//  }
 
 static esp_err_t panel_TK040F1510_init(esp_lcd_panel_t *panel)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
     // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf000,(uint8_t[]) { 0x0055,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf001,(uint8_t[]) { 0x00aa,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf002,(uint8_t[]) { 0x0052,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf003,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf004,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB000,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB001,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB002,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB600,(uint8_t[]) { 0x0046,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB601,(uint8_t[]) { 0x0046,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd100,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd101,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd102,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd103,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd104,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd105,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd106,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd107,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd108,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd109,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd10F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd110,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd111,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd112,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd113,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd114,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd115,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd116,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd117,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd118,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd119,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd11F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd120,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd121,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd122,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd123,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd124,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd125,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd126,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd127,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd128,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd129,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd12F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd130,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd131,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd132,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd133,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd200,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd201,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd202,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd203,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd204,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd205,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd206,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd207,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd208,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd209,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd20F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd210,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd211,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd212,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd213,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd214,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd215,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd216,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd217,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd218,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd219,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd21F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd220,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd221,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd222,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd223,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd224,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd225,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd226,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd227,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd228,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd229,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd22F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd230,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd231,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd232,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd233,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd300,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd301,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd302,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd303,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd304,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd305,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd306,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd307,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd308,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd309,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd30F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd310,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd311,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd312,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd313,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd314,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd315,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd316,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd317,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd318,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd319,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd31F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd320,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd321,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd322,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd323,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd324,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd325,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd326,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd327,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd328,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd329,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd32F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd330,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd331,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd332,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd333,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd400,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd401,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd402,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd403,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd404,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd405,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd406,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd407,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd408,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd409,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd40F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd410,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd411,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd412,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd413,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd414,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd415,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd416,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd417,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd418,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd419,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd41F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd420,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd421,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd422,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd423,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd424,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd425,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd426,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd427,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd428,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd429,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd42F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd430,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd431,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd432,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd433,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd500,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd501,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd502,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd503,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd504,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd505,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd506,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd507,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd508,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd509,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd50F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd510,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd511,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd512,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd513,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd514,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd515,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd516,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd517,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd518,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd519,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd51F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd520,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd521,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd522,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd523,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd524,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd525,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd526,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd527,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd528,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd529,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd52F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd530,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd531,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd532,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd533,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd600,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd601,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd602,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd603,(uint8_t[]) { 0x001C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd604,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd605,(uint8_t[]) { 0x004E,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd606,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd607,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd608,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd609,(uint8_t[]) { 0x0085,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60A,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60B,(uint8_t[]) { 0x00AB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60C,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60D,(uint8_t[]) { 0x00C4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60E,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd60F,(uint8_t[]) { 0x00FC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd610,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd611,(uint8_t[]) { 0x0023,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd612,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd613,(uint8_t[]) { 0x0061,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd614,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd615,(uint8_t[]) { 0x0094,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd616,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd617,(uint8_t[]) { 0x00E4,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd618,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd619,(uint8_t[]) { 0x0027,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61A,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61B,(uint8_t[]) { 0x0029,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61C,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61D,(uint8_t[]) { 0x0065,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61E,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd61F,(uint8_t[]) { 0x00A6,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd620,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd621,(uint8_t[]) { 0x00CA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd622,(uint8_t[]) { 0x0002,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd623,(uint8_t[]) { 0x00FD,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd624,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd625,(uint8_t[]) { 0x001D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd626,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd627,(uint8_t[]) { 0x004D,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd628,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd629,(uint8_t[]) { 0x006A,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62A,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62B,(uint8_t[]) { 0x0095,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62C,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62D,(uint8_t[]) { 0x00AC,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62E,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd62F,(uint8_t[]) { 0x00CB,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd630,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd631,(uint8_t[]) { 0x00EA,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd632,(uint8_t[]) { 0x0003,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xd633,(uint8_t[]) { 0x00EF,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xba00,(uint8_t[]) { 0x0036,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xba01,(uint8_t[]) { 0x0036,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xba02,(uint8_t[]) { 0x0036,  }, 2), TAG, "io tx param failed");  
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB900,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB901,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB902,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf000,(uint8_t[]) { 0x0055,  }, 2), TAG, "io tx param failed"); //P1
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf001,(uint8_t[]) { 0x00aa,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf002,(uint8_t[]) { 0x0052,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf003,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xf004,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb100,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xbC00,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xbC01,(uint8_t[]) { 0x0080,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xbC02,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb800,(uint8_t[]) { 0x0034,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb801,(uint8_t[]) { 0x0034,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb802,(uint8_t[]) { 0x0034,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb602,(uint8_t[]) { 0x0046,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb700,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb701,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb702,(uint8_t[]) { 0x0026,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb200,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb201,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb202,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xbF00,(uint8_t[]) { 0x0001,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb300,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb301,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb302,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb500,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb501,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xb502,(uint8_t[]) { 0x0008,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3500,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed");

ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB101,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xB102,(uint8_t[]) { 0x000C,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBD00,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBD01,(uint8_t[]) { 0x0080,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBD02,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed");
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBE00,(uint8_t[]) { 0x0000,  }, 2), TAG, "io tx param failed"); 
ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0xBE01,(uint8_t[]) { 0x0055,  }, 2), TAG, "io tx param failed"); // 6A


     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3600,(uint8_t[]) { 0x00A3,  }, 2), TAG, "io tx param failed");//这个AB的高5位是旋转屏幕用的，D3位是设置RGB与BGR顺序，低两位是相互镜像X轴与Y轴
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io,0x3A00,(uint8_t[]) { 0x0055,  }, 2), TAG, "io tx param failed");//如果用24位，则是77，如果你用16位STM32的FSMC，77改为55	
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X11 << 8, NULL, 0), TAG,"io tx param failed");
     vTaskDelay(pdMS_TO_TICKS(500));
 
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0X2900, NULL, 0), TAG,
                         "io tx param failed");vTaskDelay(pdMS_TO_TICKS(500));
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end,
                                           const void *color_data)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
 
     x_start += TK040F1510->x_gap;
     x_end += TK040F1510->x_gap;
     y_start += TK040F1510->y_gap;
     y_end += TK040F1510->y_gap;
 
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
     size_t len = (x_end - x_start) * (y_end - y_start) * TK040F1510->fb_bits_per_pixel / 8;
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR_RB424, color_data, len), TAG, "io tx color failed"); // Write frame memory
 
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
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
 
 static esp_err_t panel_TK040F1510_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
     if (mirror_x) {
         TK040F1510->madctl_val |= LCD_CMD_MX_BIT;
     } else {
         TK040F1510->madctl_val &= ~LCD_CMD_MX_BIT;
     }
     if (mirror_y) {
         TK040F1510->madctl_val |= LCD_CMD_MY_BIT;
     } else {
         TK040F1510->madctl_val &= ~LCD_CMD_MY_BIT;
     }
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
         TK040F1510->madctl_val
     }, 1), TAG, "io tx param failed");
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
     if (swap_axes) {
         TK040F1510->madctl_val |= LCD_CMD_MV_BIT;
     } else {
         TK040F1510->madctl_val &= ~LCD_CMD_MV_BIT;
     }
     ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
         TK040F1510->madctl_val
     }, 1), TAG, "io tx param failed");
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     TK040F1510->x_gap = x_gap;
     TK040F1510->y_gap = y_gap;
     return ESP_OK;
 }
 
 static esp_err_t panel_TK040F1510_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
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
 
 static esp_err_t panel_TK040F1510_sleep(esp_lcd_panel_t *panel, bool sleep)
 {
     TK040F1510_panel_t *TK040F1510 = __containerof(panel, TK040F1510_panel_t, base);
     esp_lcd_panel_io_handle_t io = TK040F1510->io;
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
 