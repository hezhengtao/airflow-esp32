#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"



#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"


#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_lcd_new_panel_TK019F1935(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif