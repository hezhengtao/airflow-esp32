#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_uri[128];
    char mqtt_user[33];
    char mqtt_pass[33];
    char device_name[33];
    char device_id[17];
    uint8_t motor_speed_min;   /* percent, default 20 */
    uint8_t motor_speed_max;   /* percent, default 100 */
    uint16_t tvoc_alarm_ugm3;
    uint16_t co2_alarm_ppm;
    uint16_t ch2o_alarm_ugm3;
    bool auto_fan_enable;      /* auto-start fan on alarm */
    uint8_t brightness;        /* percent, default 80 */
    bool child_lock;
    uint8_t key_volume;        /* percent, default 50 */
    bool key_sound_enable;     /* key/button click sound */
    uint8_t power_volume;      /* percent, default 70 */
    bool power_sound_enable;   /* power on/off melody */
    uint8_t alarm_volume;      /* percent, default 70 */
    bool alarm_sound_enable;   /* alarm sound on/off */
    uint16_t alarm_cooldown_s; /* alarm cooldown seconds, default 60 */
    uint8_t home_screen;       /* default UI_SCREEN_HOME (0) */
    uint8_t key_melody;        /* key-click melody index 0-3 */
    uint8_t power_on_melody;   /* power-on melody index 0-3 */
    uint8_t power_off_melody;  /* power-off melody index 0-3 */
    uint8_t alarm_melody;      /* alarm melody index 0-5 */
} app_settings_t;

void settings_init(void);
void settings_load(app_settings_t *out);
void settings_save(const app_settings_t *s);
void settings_factory_defaults(app_settings_t *out);
void settings_save_str(const char *key, const char *value);
void settings_save_u8(const char *key, uint8_t value);
void settings_save_u32(const char *key, uint32_t value);
esp_err_t settings_get_str(const char *key, char *out, size_t max_len);
esp_err_t settings_get_u8(const char *key, uint8_t *out);
esp_err_t settings_get_u32(const char *key, uint32_t *out);
void settings_erase_key(const char *key);
void settings_commit(void);
void settings_erase_all(void);
