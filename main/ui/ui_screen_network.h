#pragma once
#include "lvgl.h"

lv_obj_t *ui_screen_network_create(lv_obj_t *parent);
void ui_screen_network_lang_update(void);
void ui_screen_network_theme_update(void);
void ui_screen_network_on_enter(void);
void ui_screen_network_boot_scan(void);
void ui_screen_network_hide_keyboard(void);
void qr_update_url(void);

extern lv_obj_t *ui_net_wifi_label;
extern lv_obj_t *ui_net_ip_label;
extern lv_obj_t *ui_net_mqtt_label;
extern lv_obj_t *ui_net_ssid_ta;
extern lv_obj_t *ui_net_pass_ta;
extern lv_obj_t *ui_net_theme_btn;
