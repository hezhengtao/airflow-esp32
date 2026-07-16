#pragma once
#include "lvgl.h"

lv_obj_t *ui_screen_brightness_create(lv_obj_t *parent);
void ui_screen_brightness_lang_update(void);

extern lv_obj_t *ui_bright_slider;
extern lv_obj_t *ui_bright_pct_label;
