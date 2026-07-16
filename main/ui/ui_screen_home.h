#pragma once
#include "lvgl.h"
#include <stdint.h>

lv_obj_t *ui_screen_home_create(lv_obj_t *parent);
void ui_screen_home_lang_update(void);
void ui_screen_home_theme_update(void);

/* Data update helpers — called from ui_manager.c */
void ui_home_update_temp(float temp_c);
void ui_home_update_tvoc(uint16_t val);
void ui_home_update_co2(uint16_t val);
void ui_home_update_ch2o(uint16_t val);
void ui_home_update_fan_speed(uint8_t pct);
void ui_home_update_fan_rpm(uint16_t rpm);
void ui_home_update_holiday(void);

/* Widgets updated by ui_manager data callbacks */
extern lv_obj_t *ui_home_temp_label;
extern lv_obj_t *ui_home_tvoc_label;
extern lv_obj_t *ui_home_co2_label;
extern lv_obj_t *ui_home_ch2o_label;
extern lv_obj_t *ui_home_fan_pct_label;
extern lv_obj_t *ui_home_fan_rpm_label;
extern lv_obj_t *ui_home_fan_slider;
extern lv_obj_t *ui_home_fan_btn;
extern lv_obj_t *ui_home_fan_btn_label;
