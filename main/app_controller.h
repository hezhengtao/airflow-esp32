#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    APP_STATE_INIT = 0,
    APP_STATE_WIFI_CONNECTING,
    APP_STATE_RUNNING,
    APP_STATE_SERVICE,
    APP_STATE_ERROR,
} app_state_t;

void app_controller_init(void);
void app_controller_start(void);
app_state_t app_controller_get_state(void);
float app_controller_get_indoor_temp(void);

void app_controller_toggle_power(void);
void app_controller_set_fan_speed(uint8_t speed_pct);
void app_controller_factory_reset(void);
void app_controller_wifi_scan(void);
void app_controller_wifi_connect(const char *ssid, const char *pass);
void app_controller_start_provisioning(void);
void app_controller_reset_provisioning(void);
void app_controller_set_alarm_threshold(uint16_t tvoc_ugm3, uint16_t co2_ppm, uint16_t ch2o_ugm3);
void app_controller_set_auto_fan(bool enable);
bool app_controller_get_auto_fan(void);
void app_controller_get_alarm_thresholds(uint16_t *tvoc, uint16_t *co2, uint16_t *ch2o);
void app_controller_screen_off(void);
void app_controller_screen_on(void);
void app_controller_set_brightness(uint8_t pct);
uint8_t app_controller_get_brightness(void);

/* Sound settings (centralized — persists to NVS + publishes MQTT) */
void app_controller_set_key_sound(bool enabled);
void app_controller_set_power_sound(bool enabled);
void app_controller_set_key_volume(uint8_t pct);
void app_controller_set_power_volume(uint8_t pct);
bool app_controller_get_key_sound(void);
bool app_controller_get_power_sound(void);
uint8_t app_controller_get_key_volume(void);
uint8_t app_controller_get_power_volume(void);

/* Alarm sound settings */
void app_controller_set_alarm_sound(bool enabled);
bool app_controller_get_alarm_sound(void);
void app_controller_set_alarm_volume(uint8_t pct);
uint8_t app_controller_get_alarm_volume(void);
void app_controller_set_alarm_cooldown(uint16_t seconds);
uint16_t app_controller_get_alarm_cooldown(void);

/* Melody selection (persists to NVS + publishes MQTT) */
void app_controller_set_key_melody(uint8_t idx);
void app_controller_set_power_on_melody(uint8_t idx);
void app_controller_set_power_off_melody(uint8_t idx);
uint8_t app_controller_get_key_melody(void);
uint8_t app_controller_get_power_on_melody(void);
uint8_t app_controller_get_power_off_melody(void);

/* Sound preview (plays only, does NOT persist or publish MQTT) */
void app_controller_preview_key_melody(uint8_t idx);
void app_controller_preview_power_on_melody(uint8_t idx);
void app_controller_preview_power_off_melody(uint8_t idx);
void app_controller_set_alarm_melody(uint8_t idx);
uint8_t app_controller_get_alarm_melody(void);
void app_controller_preview_alarm_melody(uint8_t idx);

void app_controller_sleep(void);
void app_controller_shutdown(void);   /* stop fan+sensors+display, stay alive for double-tap wake */
void app_controller_wake(void);       /* clear shutdown flag, called on double-tap wake */
bool app_controller_is_shutdown(void); /* true when in soft-shutdown state */

/* Cross-task power action request (for web/mqtt → LVGL task dispatch) */
typedef enum {
    POWER_ACTION_NONE = 0,
    POWER_ACTION_SCREEN_OFF,
    POWER_ACTION_SHUTDOWN,
    POWER_ACTION_WAKE,
} power_action_t;
void app_controller_request_power_action(power_action_t action);
power_action_t app_controller_consume_power_action(void);
bool app_controller_is_wifi_ready(void);  /* true after esp_wifi_init() succeeds */
void app_controller_set_status_led(bool on);
bool app_controller_get_status_led(void);
void app_controller_set_led_rgb(uint8_t r, uint8_t g, uint8_t b);
void app_controller_get_led_rgb(uint8_t *r, uint8_t *g, uint8_t *b);
void app_controller_set_led_brightness(uint8_t pct);
uint8_t app_controller_get_led_brightness(void);
void app_controller_set_led_effect(uint8_t effect);

/* Per-state LED config (5 states, index = led_state_id_t) */
typedef enum {
    LED_STATE_NORMAL = 0,
    LED_STATE_ALARM,
    LED_STATE_SHUTDOWN,
    LED_STATE_WIFI_FAIL,
    LED_STATE_WIFI_CONNECTING,
    LED_STATE_OTA_UPGRADE,
    LED_STATE_COUNT
} led_state_id_t;

typedef struct {
    uint8_t r, g, b;
    uint8_t effect;  /* 0=steady, 1=blink, 2=breathe */
} led_state_cfg_t;

void app_controller_set_led_state_cfg(led_state_id_t id, const led_state_cfg_t *cfg);
const led_state_cfg_t *app_controller_get_led_state_cfg(led_state_id_t id);

/* HA per-state LED control — mirrors web "状态指示灯" card */
void app_controller_set_led_cfg_state(uint8_t state_id);
uint8_t app_controller_get_led_cfg_state(void);
void app_controller_set_led_state_rgb(uint8_t state_id, uint8_t r, uint8_t g, uint8_t b);
void app_controller_set_led_state_effect(uint8_t state_id, uint8_t eff);

void app_controller_set_ota_in_progress(bool in_progress);
void app_controller_mqtt_apply(void);
