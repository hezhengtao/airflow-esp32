#pragma once
#include "lvgl.h"

lv_obj_t *ui_screen_settings_create(lv_obj_t *parent);
void ui_screen_settings_lang_update(void);
void ui_screen_settings_theme_update(void);
void ui_settings_update_brightness(uint8_t pct);
