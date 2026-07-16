#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_types.h"

typedef enum {
    UI_SCREEN_HOME = 0,
    UI_SCREEN_NETWORK,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_SOUND,
    UI_SCREEN_POWER,
    UI_SCREEN_COUNT
} ui_screen_t;

void ui_manager_init(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle);
void ui_manager_show(void);
void ui_manager_show_first_screen(void);
void ui_manager_navigate_to(ui_screen_t screen);
ui_screen_t ui_manager_get_current(void);
void ui_manager_apply_theme(void);
void ui_manager_screen_off(void);   /* blank display, double-tap to wake */
void ui_manager_shutdown(void);     /* stop fan+sensors+display, double-tap to wake */
void ui_manager_wake(void);         /* wake from screen-off or shutdown */
bool ui_manager_is_swipe_blocked(void);  /* true for 500ms after swipe — prevents accidental clicks */

void ui_update_temperature(float temp_c);
void ui_update_humidity(float humidity_pct);
void ui_update_tvoc(uint16_t tvoc_ugm3);
void ui_update_co2(uint16_t co2_ppm);
void ui_update_ch2o(uint16_t ch2o_ugm3);
void ui_update_fan_speed(uint8_t speed_pct);
void ui_update_fan_rpm(uint16_t rpm);
void ui_update_fan_state(bool on);
void ui_update_wifi_status(bool connected, const char *ssid);
void ui_update_mqtt_status(bool connected);
void ui_update_ip(const char *ip);

void ui_show_alert(const char *title, const char *message);
