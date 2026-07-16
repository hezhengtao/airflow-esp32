#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LCD in RGB mode (16-bit, RGB565).
 *
 * Creates an esp_lcd_rgb_panel with PSRAM framebuffer.
 * No panel IO handle needed — RGB writes directly via DMA.
 *
 * @param ret_panel [out] Pointer to receive the panel handle
 * @return ESP_OK on success
 */
esp_err_t lcd_init_rgb(esp_lcd_panel_handle_t *ret_panel);

void lcd_set_backlight(uint8_t duty_percent);
void lcd_display_on(void);

#ifdef __cplusplus
}
#endif
