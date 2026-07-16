#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t lcd_init(esp_lcd_panel_io_handle_t *io_handle,
                   esp_lcd_panel_handle_t *panel_handle);
void lcd_set_backlight(uint8_t duty_percent);
void lcd_display_on(void);

#ifdef __cplusplus
}
#endif
