#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"



#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"

#define H_Start_Address             0x50 // Set column address
#define H_End_Address               0x51 // Set column address
#define V_Start_Address             0x52 // Set row address
#define V_End_Address               0x53 // Set row address
#define LCD_CMD_RAMWR_RB424         0x2C00 // Write frame memory

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_lcd_new_panel_TK020RB424(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif