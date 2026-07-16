#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
} mqtt_state_t;

typedef void (*mqtt_cmd_cb_t)(const char *topic, const char *payload, int payload_len, void *user_data);
typedef void (*mqtt_connected_cb_t)(void *user_data);

void mqtt_ha_init(const char *broker_uri, const char *username, const char *password,
                  const char *device_name, const char *device_id);
void mqtt_ha_start(void);
void mqtt_ha_stop(void);
mqtt_state_t mqtt_ha_get_state(void);
void mqtt_ha_set_command_callback(mqtt_cmd_cb_t cb, void *user_data);
void mqtt_ha_set_connected_callback(mqtt_connected_cb_t cb, void *user_data);

/* Publish sensor data */
void mqtt_ha_publish_temperature(float temp_c);
void mqtt_ha_publish_tvoc(uint16_t tvoc_ugm3);
void mqtt_ha_publish_co2(uint16_t co2_ppm);
void mqtt_ha_publish_ch2o(uint16_t ch2o_ugm3);
void mqtt_ha_publish_fan_state(bool on, uint8_t speed_pct);
void mqtt_ha_publish_alarm(uint8_t type, uint16_t value, uint16_t threshold);

/* WiFi info */
void mqtt_ha_publish_wifi_info(int8_t rssi, const char *ssid, const char *ip);

/* Device uptime */
void mqtt_ha_publish_uptime(uint32_t seconds);

/* Switches (state feedback) */
void mqtt_ha_publish_auto_fan(bool enabled);
void mqtt_ha_publish_key_sound(bool enabled);
void mqtt_ha_publish_power_sound(bool enabled);
void mqtt_ha_publish_alarm_sound(bool enabled);
void mqtt_ha_publish_alarm_volume(uint8_t pct);
void mqtt_ha_publish_alarm_cooldown(uint16_t seconds);

/* Per-state LED config (HA mirrors web "状态指示灯" card) */
void mqtt_ha_publish_led_cfg_state_name(const char *name);
void mqtt_ha_publish_led_state_rgb(uint8_t r, uint8_t g, uint8_t b);
void mqtt_ha_publish_led_state_eff(uint8_t eff);
void mqtt_ha_publish_led_state_eff_name(const char *name);

/* Numbers (state feedback) */
void mqtt_ha_publish_brightness(uint8_t pct);
void mqtt_ha_publish_key_volume(uint8_t pct);
void mqtt_ha_publish_power_volume(uint8_t pct);
void mqtt_ha_publish_tvoc_threshold(uint16_t ugm3);
void mqtt_ha_publish_co2_threshold(uint16_t ppm);
void mqtt_ha_publish_ch2o_threshold(uint16_t ugm3);

/* LED state feedback */
void mqtt_ha_publish_led_on(bool on);
void mqtt_ha_publish_led_rgb(uint8_t r, uint8_t g, uint8_t b);
void mqtt_ha_publish_led_brightness(uint8_t pct);

/* Binary sensors */
void mqtt_ha_publish_power_state(bool on);
void mqtt_ha_publish_schedule_active(bool active);

/* Holiday name (from timor.tech API) */
void mqtt_ha_publish_holiday(const char *name);

/* Clear stale retained value for a subtopic */
void mqtt_ha_publish_clear(const char *subtopic);

typedef void (*mqtt_disconnected_cb_t)(void *user_data);
void mqtt_ha_set_disconnected_callback(mqtt_disconnected_cb_t cb, void *user_data);
void mqtt_ha_publish_fan_rpm(uint16_t rpm);
void mqtt_ha_publish_fan_preset(uint8_t speed_pct);
void mqtt_ha_publish_key_melody(uint8_t idx);
void mqtt_ha_publish_key_melody_name(const char *name);
void mqtt_ha_publish_power_on_melody(uint8_t idx);
void mqtt_ha_publish_power_on_melody_name(const char *name);
void mqtt_ha_publish_power_off_melody(uint8_t idx);
void mqtt_ha_publish_power_off_melody_name(const char *name);
void mqtt_ha_publish_alarm_melody_name(const char *name);
void mqtt_ha_publish_home_screen_name(const char *name);
void mqtt_ha_publish_sched_off_time(const char *t);
void mqtt_ha_publish_sched_on_time(const char *t);
void mqtt_ha_publish_sched_off_day(uint8_t d);
void mqtt_ha_publish_sched_on_day(uint8_t d);
void mqtt_ha_publish_sched_off_en(bool en);
void mqtt_ha_publish_sched_on_en(bool en);
void mqtt_ha_publish_schedules_json(const char *json);

/* HomeAssistant auto-discovery */
void mqtt_ha_publish_discovery(void);
