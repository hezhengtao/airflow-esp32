#include "mqtt_ha.h"
#include "board.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mqtt_ha";

static esp_mqtt_client_handle_t g_client = NULL;
static mqtt_state_t g_state = MQTT_STATE_DISCONNECTED;
static mqtt_cmd_cb_t g_cmd_cb = NULL;
static void *g_cmd_user_data = NULL;
static mqtt_connected_cb_t g_connected_cb = NULL;
static void *g_connected_user_data = NULL;
static mqtt_disconnected_cb_t g_disconnected_cb = NULL;
static void *g_disconnected_user_data = NULL;

static char g_device_name[33] = "Air Purifier";
static char g_device_id[17] = "esp32_ap_01";
static char g_base_topic[64] = "air_purifier";

static void publish_string(const char *subtopic, const char *value)
{
    if (!g_client || g_state != MQTT_STATE_CONNECTED) return;
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s", g_base_topic, subtopic);
    esp_mqtt_client_enqueue(g_client, topic, value, 0, 1, true, true);
}

void mqtt_ha_publish_clear(const char *subtopic)
{
    /* Zero-length payload + retain clears the retained topic */
    publish_string(subtopic, "");
}

static void publish_float(const char *subtopic, float value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", value);
    publish_string(subtopic, buf);
}

static void publish_int(const char *subtopic, uint16_t value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", value);
    publish_string(subtopic, buf);
}

/* ── Discovery publish helper — uses esp_mqtt_client_publish (synchronous,
 *   bypasses outbox) to avoid outbox overflow when publishing many entities. ── */
static int disc_pub(const char *topic, const char *payload)
{
    int ret = esp_mqtt_client_publish(g_client, topic, payload, 0, 0, true);
    if (ret < 0) {
        ESP_LOGW(TAG, "disc_pub fail %d: %s", ret, topic);
    }
    return ret;
}

/* ── Discovery helpers ─────────────────────────────────────────────── */

static void publish_fan_discovery(void)
{
    char topic[128], payload[1200];
    snprintf(topic, sizeof(topic),
        "homeassistant/fan/%s/config", g_device_id);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s 风扇\","
        "\"unique_id\":\"%s_fan\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/fan_state\","
        "\"command_topic\":\"%s/fan_cmd\","
        "\"percentage_state_topic\":\"%s/fan_speed\","
        "\"percentage_command_topic\":\"%s/fan_set_speed\","
        "\"preset_modes\":[\"1\",\"2\",\"3\",\"4\",\"5\"],"
        "\"preset_mode_state_topic\":\"%s/fan_preset\","
        "\"preset_mode_command_topic\":\"%s/fan_preset_cmd\","
        "\"speed_range_min\":1,\"speed_range_max\":100,"
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
        "\"icon\":\"mdi:fan\""
        "}",
        g_device_name, g_device_id,
        g_device_name, g_device_id,
        g_base_topic, g_base_topic,
        g_base_topic, g_base_topic,
        g_base_topic, g_base_topic);

    disc_pub(topic, payload);
}

static void publish_sensor_discovery(const char *sensor_id, const char *name,
                                     const char *unit, const char *dev_class,
                                     const char *icon, const char *ent_cat)
{
    char topic[128], payload[640];
    snprintf(topic, sizeof(topic),
        "homeassistant/sensor/%s_%s/config", g_device_id, sensor_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\","
        "\"unit_of_measurement\":\"%s\","
        "\"device_class\":\"%s\""
        "%s%s"
        "}",
        g_device_name, name,
        g_device_id, sensor_id,
        g_device_name, g_device_id,
        g_base_topic, sensor_id,
        unit, dev_class,
        icon_str, cat_str);

    disc_pub(topic, payload);
}

static void publish_text_sensor_discovery(const char *sensor_id, const char *name,
                                          const char *icon, const char *ent_cat)
{
    char topic[128], payload[512];
    snprintf(topic, sizeof(topic),
        "homeassistant/sensor/%s_%s/config", g_device_id, sensor_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\""
        "%s%s"
        "}",
        g_device_name, name,
        g_device_id, sensor_id,
        g_device_name, g_device_id,
        g_base_topic, sensor_id,
        icon_str, cat_str);

    disc_pub(topic, payload);
}

static void publish_switch_discovery(const char *switch_id, const char *name,
                                      const char *state_t, const char *cmd_t,
                                      const char *icon, const char *ent_cat)
{
    char topic[128], payload[640];
    snprintf(topic, sizeof(topic),
        "homeassistant/switch/%s_%s/config", g_device_id, switch_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\","
        "\"command_topic\":\"%s/%s\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\""
        "%s%s"
        "}",
        g_device_name, name,
        g_device_id, switch_id,
        g_device_name, g_device_id,
        g_base_topic, state_t, g_base_topic, cmd_t,
        icon_str, cat_str);

    disc_pub(topic, payload);
}

static void publish_number_discovery(const char *num_id, const char *name,
                                      const char *unit, int min, int max,
                                      const char *state_t, const char *cmd_t,
                                      const char *icon, const char *ent_cat)
{
    char topic[128], payload[1024];
    snprintf(topic, sizeof(topic),
        "homeassistant/number/%s_%s/config", g_device_id, num_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\","
        "\"command_topic\":\"%s/%s\","
        "\"min\":%d,\"max\":%d,"
        "\"unit_of_measurement\":\"%s\""
        "%s%s"
        "}",
        g_device_name, name,
        g_device_id, num_id,
        g_device_name, g_device_id,
        g_base_topic, state_t, g_base_topic, cmd_t,
        min, max, unit,
        icon_str, cat_str);

    disc_pub(topic, payload);
}

static void publish_binary_sensor_discovery(const char *bs_id, const char *name,
                                             const char *state_t, const char *dev_class,
                                             const char *icon, const char *ent_cat)
{
    char topic[128], payload[640];
    snprintf(topic, sizeof(topic),
        "homeassistant/binary_sensor/%s_%s/config", g_device_id, bs_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);
    char dc_str[64] = "";
    if (dev_class && dev_class[0]) snprintf(dc_str, sizeof(dc_str), ",\"device_class\":\"%s\"", dev_class);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\","
        "\"payload_on\":\"ON\",\"payload_off\":\"OFF\""
        "%s%s%s"
        "}",
        g_device_name, name,
        g_device_id, bs_id,
        g_device_name, g_device_id,
        g_base_topic, state_t,
        icon_str, cat_str, dc_str);

    disc_pub(topic, payload);
}

static void publish_select_discovery(const char *sel_id, const char *name,
                                      const char *options_json,
                                      const char *state_t, const char *cmd_t,
                                      const char *icon, const char *ent_cat)
{
    char topic[128], payload[1024];
    snprintf(topic, sizeof(topic),
        "homeassistant/select/%s_%s/config", g_device_id, sel_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);
    char cat_str[80] = "";
    if (ent_cat && ent_cat[0]) snprintf(cat_str, sizeof(cat_str), ",\"entity_category\":\"%s\"", ent_cat);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"state_topic\":\"%s/%s\","
        "\"command_topic\":\"%s/%s\","
        "\"options\":%s"
        "%s%s"
        "}",
        g_device_name, name,
        g_device_id, sel_id,
        g_device_name, g_device_id,
        g_base_topic, state_t, g_base_topic, cmd_t,
        options_json,
        icon_str, cat_str);

    disc_pub(topic, payload);
}

static void publish_button_discovery(const char *btn_id, const char *name,
                                      const char *cmd_t, const char *icon)
{
    char topic[128], payload[640];
    snprintf(topic, sizeof(topic),
        "homeassistant/button/%s_%s/config", g_device_id, btn_id);

    char icon_str[64] = "";
    if (icon && icon[0]) snprintf(icon_str, sizeof(icon_str), ",\"icon\":\"%s\"", icon);

    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s %s\","
        "\"unique_id\":\"%s_%s\","
        "\"device\":{\"name\":\"%s\",\"identifiers\":[\"%s\"],\"manufacturer\":\"AiRFLOW\",\"model\":\"空气净化器\"},"
        "\"command_topic\":\"%s/%s\""
        "%s"
        "}",
        g_device_name, name,
        g_device_id, btn_id,
        g_device_name, g_device_id,
        g_base_topic, cmd_t,
        icon_str);

    disc_pub(topic, payload);
}

/* ── MQTT event handler ────────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        g_state = MQTT_STATE_CONNECTED;
        char sub_topic[80];
        snprintf(sub_topic, sizeof(sub_topic), "%s/#", g_base_topic);
        esp_mqtt_client_subscribe(g_client, sub_topic, 0);
        /* Run discovery inline — disc_pub() uses synchronous
         * esp_mqtt_client_publish (bypasses outbox), so no delays needed. */
        mqtt_ha_publish_discovery();
        if (g_connected_cb) g_connected_cb(g_connected_user_data);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected from broker");
        g_state = MQTT_STATE_DISCONNECTED;
        if (g_disconnected_cb) g_disconnected_cb(g_disconnected_user_data);
        break;

    case MQTT_EVENT_DATA:
        if (g_cmd_cb && ev->topic && ev->data) {
            g_cmd_cb(ev->topic, ev->data, ev->data_len, g_cmd_user_data);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ── Init / Start / Stop ───────────────────────────────────────────── */

void mqtt_ha_init(const char *broker_uri, const char *username,
                  const char *password, const char *device_name,
                  const char *device_id)
{
    if (device_name) strncpy(g_device_name, device_name, sizeof(g_device_name) - 1);
    if (device_id) strncpy(g_device_id, device_id, sizeof(g_device_id) - 1);
    snprintf(g_base_topic, sizeof(g_base_topic), "air_purifier/%s", g_device_id);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .session.keepalive = 60,
        .buffer.size = 3072,
        .buffer.out_size = 3072,
        .outbox.limit = 16384,
        .session.last_will.topic = g_base_topic,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .task.stack_size = 4608,
    };

    g_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(g_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "Initialized: broker=%s device=%s", broker_uri, g_device_name);
}

void mqtt_ha_start(void)
{
    if (!g_client) return;
    g_state = MQTT_STATE_CONNECTING;
    esp_mqtt_client_start(g_client);
}

void mqtt_ha_stop(void)
{
    if (!g_client) return;
    publish_string("status", "offline");
    esp_mqtt_client_stop(g_client);
    g_state = MQTT_STATE_DISCONNECTED;
}

mqtt_state_t mqtt_ha_get_state(void) { return g_state; }

void mqtt_ha_set_command_callback(mqtt_cmd_cb_t cb, void *user_data)
{
    g_cmd_cb = cb;
    g_cmd_user_data = user_data;
}

void mqtt_ha_set_connected_callback(mqtt_connected_cb_t cb, void *user_data)
{
    g_connected_cb = cb;
    g_connected_user_data = user_data;
}

/* ── Sensor publishes ──────────────────────────────────────────────── */

void mqtt_ha_publish_temperature(float temp_c) {
    publish_float("temperature", temp_c);
}

void mqtt_ha_publish_tvoc(uint16_t tvoc_ugm3) {
    publish_int("tvoc", tvoc_ugm3);
}

void mqtt_ha_publish_co2(uint16_t co2_ppm) {
    publish_int("co2", co2_ppm);
}

void mqtt_ha_publish_ch2o(uint16_t ch2o_ugm3) {
    publish_int("ch2o", ch2o_ugm3);
}

void mqtt_ha_publish_fan_state(bool on, uint8_t speed_pct) {
    publish_string("fan_state", on ? "ON" : "OFF");
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", speed_pct);
    publish_string("fan_speed", buf);
}

void mqtt_ha_publish_alarm(uint8_t type, uint16_t value, uint16_t threshold)
{
    const char *names[] = {"TVOC", "CO2", "CH2O"};
    const char *name = (type < 3) ? names[type] : "未知";
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"%s\",\"value\":%u,\"threshold\":%u}", name, value, threshold);
    publish_string("alarm", buf);
}


/* ── WiFi info ─────────────────────────────────────────────────────── */

void mqtt_ha_publish_wifi_info(int8_t rssi, const char *ssid, const char *ip)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", rssi);
    publish_string("wifi_rssi", buf);
    if (ssid) publish_string("wifi_ssid", ssid);
    if (ip) publish_string("wifi_ip", ip);
}

/* ── Binary sensors ──────────────────────────────────────────────────── */

void mqtt_ha_publish_power_state(bool on) {
    publish_string("power_state", on ? "ON" : "OFF");
}

void mqtt_ha_publish_schedule_active(bool active) {
    publish_string("schedule_active", active ? "ON" : "OFF");
}

/* ── Holiday ─────────────────────────────────────────────────────────── */

void mqtt_ha_publish_holiday(const char *name)
{
    if (name && name[0]) {
        publish_string("holiday", name);
    } else {
        publish_string("holiday", "");
    }
}

/* ── Uptime ────────────────────────────────────────────────────────── */

void mqtt_ha_publish_uptime(uint32_t seconds)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)seconds);
    publish_string("uptime", buf);
}

/* ── Switches ──────────────────────────────────────────────────────── */

void mqtt_ha_publish_auto_fan(bool enabled) {
    publish_string("auto_fan", enabled ? "ON" : "OFF");
}

void mqtt_ha_publish_key_sound(bool enabled) {
    publish_string("key_sound", enabled ? "ON" : "OFF");
}

void mqtt_ha_publish_power_sound(bool enabled) {
    publish_string("power_sound", enabled ? "ON" : "OFF");
}

void mqtt_ha_publish_alarm_sound(bool enabled) {
    publish_string("alarm_sound", enabled ? "ON" : "OFF");
}

void mqtt_ha_publish_alarm_volume(uint8_t pct) {
    publish_int("alarm_volume", pct);
}
void mqtt_ha_publish_alarm_cooldown(uint16_t seconds) {
    publish_int("alarm_cooldown", seconds);
}

/* ── Per-state LED config ──────────────────────────────────────────── */

void mqtt_ha_publish_led_cfg_state_name(const char *name) {
    publish_string("led_cfg_state", name);
}

void mqtt_ha_publish_led_state_rgb(uint8_t r, uint8_t g, uint8_t b) {
    publish_int("led_state_r", r);
    publish_int("led_state_g", g);
    publish_int("led_state_b", b);
}

void mqtt_ha_publish_led_state_eff(uint8_t eff) {
    publish_int("led_state_eff", eff);
}
void mqtt_ha_publish_led_state_eff_name(const char *name) {
    publish_string("led_state_eff", name);
}

/* ── Numbers ───────────────────────────────────────────────────────── */

void mqtt_ha_publish_brightness(uint8_t pct) {
    publish_int("brightness", pct);
}

void mqtt_ha_publish_key_volume(uint8_t pct) {
    publish_int("key_volume", pct);
}

void mqtt_ha_publish_power_volume(uint8_t pct) {
    publish_int("power_volume", pct);
}

void mqtt_ha_publish_tvoc_threshold(uint16_t ugm3) {
    publish_int("tvoc_alarm", ugm3);
}

void mqtt_ha_publish_co2_threshold(uint16_t ppm) {
    publish_int("co2_alarm", ppm);
}

void mqtt_ha_publish_ch2o_threshold(uint16_t ugm3) {
    publish_int("ch2o_alarm", ugm3);
}

/* ── LED state ─────────────────────────────────────────────────────── */

void mqtt_ha_publish_led_on(bool on) {
    publish_string("led_on", on ? "ON" : "OFF");
}

void mqtt_ha_publish_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    publish_int("led_r", r);
    publish_int("led_g", g);
    publish_int("led_b", b);
}

void mqtt_ha_publish_led_brightness(uint8_t pct) {
    publish_int("led_brightness", pct);
}

/* ── HomeAssistant Discovery ───────────────────────────────────────── */

void mqtt_ha_publish_discovery(void)
{
    ESP_LOGI(TAG, "Discovery: starting...");

    publish_fan_discovery();
    /* Standalone speed number — always works regardless of HA fan support */
    publish_number_discovery("fan_speed_pct", "风扇转速", "%", 1, 100,
                             "fan_speed", "fan_set_speed",
                             "mdi:fan-speed-3", "");

    /* ── Core sensors (main area) ── */
    publish_sensor_discovery("temperature", "温度", "°C", "temperature", "", "");
    publish_sensor_discovery("tvoc", "TVOC", "µg/m³", "volatile_organic_compounds",
                             "mdi:air-filter", "");
    publish_sensor_discovery("co2", "CO₂", "ppm", "carbon_dioxide",
                             "mdi:molecule-co2", "");
    publish_sensor_discovery("ch2o", "甲醛", "µg/m³", "volatile_organic_compounds",
                             "mdi:biohazard", "");

    /* ── WiFi info (diagnostic) ── */
    publish_sensor_discovery("wifi_rssi", "WiFi信号强度", "dBm", "signal_strength",
                             "", "diagnostic");
    publish_sensor_discovery("wifi_ssid", "WiFi名称", "", "",
                             "mdi:wifi-check", "diagnostic");
    publish_sensor_discovery("wifi_ip", "IP地址", "", "",
                             "mdi:ip", "diagnostic");


    /* ── Uptime (diagnostic) ── */
    publish_sensor_discovery("uptime", "运行时间", "s", "duration",
                             "mdi:timer-outline", "diagnostic");

    /* ── Holiday (text sensor, from timor.tech API) ── */
    publish_text_sensor_discovery("holiday", "节假日",
                                  "mdi:party-popper", "diagnostic");

    /* ── Switches ── */
    publish_switch_discovery("auto_fan", "自动风扇", "auto_fan", "auto_fan_cmd",
                             "mdi:fan-auto", "");
    publish_switch_discovery("key_sound", "按键音", "key_sound", "key_sound_cmd",
                             "mdi:volume-high", "config");
    publish_switch_discovery("power_sound", "开关机音效", "power_sound", "power_sound_cmd",
                             "mdi:power-plug", "config");
    publish_switch_discovery("alarm_sound", "报警音效", "alarm_sound", "alarm_sound_cmd",
                             "mdi:alarm-bell", "config");

    /* ── Numbers ── */
    publish_number_discovery("brightness", "屏幕亮度", "%", 10, 100,
                             "brightness", "brightness_cmd",
                             "mdi:brightness-6", "config");
    publish_number_discovery("key_volume", "按键音量", "%", 0, 100,
                             "key_volume", "key_volume_cmd",
                             "mdi:volume-high", "config");
    publish_number_discovery("power_volume", "开关机音量", "%", 0, 100,
                             "power_volume", "power_volume_cmd",
                             "mdi:volume-high", "config");
    publish_number_discovery("alarm_volume", "报警音量", "%", 0, 100,
                             "alarm_volume", "alarm_volume_cmd",
                             "mdi:volume-high", "config");
    publish_number_discovery("alarm_cooldown", "报警冷却时间", "s", 5, 600,
                             "alarm_cooldown", "alarm_cooldown_cmd",
                             "mdi:timer-sand", "config");
    publish_number_discovery("tvoc_alarm", "TVOC报警阈值", "µg/m³", 100, 2000,
                             "tvoc_alarm", "alarm_tvoc_cmd",
                             "mdi:alert-circle", "config");
    publish_number_discovery("co2_alarm", "CO₂报警阈值", "ppm", 400, 5000,
                             "co2_alarm", "alarm_co2_cmd",
                             "mdi:alert-circle", "config");
    publish_number_discovery("ch2o_alarm", "甲醛报警阈值", "µg/m³", 20, 500,
                             "ch2o_alarm", "alarm_ch2o_cmd",
                             "mdi:alert-circle", "config");

    /* ── Binary sensors ── */
    publish_binary_sensor_discovery("power_state", "开关机状态",
                                    "power_state", "power", "mdi:power", "");
    publish_binary_sensor_discovery("schedule_active", "定时任务状态",
                                    "schedule_active", "running", "mdi:clock-outline", "");

    /* ── Buttons ── */
    publish_button_discovery("restart", "重启设备", "restart_cmd",
                             "mdi:restart");
    publish_button_discovery("screen_off", "关屏幕", "screen_off_cmd",
                             "mdi:monitor-off");
    publish_button_discovery("screen_on", "开屏幕", "screen_on_cmd",
                             "mdi:monitor");
    publish_button_discovery("shutdown", "关机", "shutdown_cmd",
                             "mdi:power");
    publish_button_discovery("wake", "开机", "wake_cmd",
                             "mdi:power-on");



    publish_string("status", "online");
    ESP_LOGI(TAG, "Discovery: complete (fan + 30 entities)");
}

void mqtt_ha_set_disconnected_callback(mqtt_disconnected_cb_t cb, void *user_data)
{ g_disconnected_cb = cb; g_disconnected_user_data = user_data; }

void mqtt_ha_publish_fan_rpm(uint16_t rpm) {
    char b[16]; snprintf(b, sizeof(b), "%u", rpm); publish_string("fan_rpm", b);
}
void mqtt_ha_publish_fan_preset(uint8_t speed_pct) {
    const char *preset = "1";
    if (speed_pct >= 81) preset = "5";
    else if (speed_pct >= 61) preset = "4";
    else if (speed_pct >= 41) preset = "3";
    else if (speed_pct >= 21) preset = "2";
    else if (speed_pct == 0) preset = "";
    publish_string("fan_preset", preset);
}
void mqtt_ha_publish_key_melody(uint8_t idx) {
    char b[4]; snprintf(b, sizeof(b), "%u", idx); publish_string("key_melody", b);
}

void mqtt_ha_publish_key_melody_name(const char *name) { publish_string("key_melody", name); }
void mqtt_ha_publish_power_on_melody(uint8_t idx) {
    char b[4]; snprintf(b, sizeof(b), "%u", idx); publish_string("pwon_melody", b);
}
void mqtt_ha_publish_power_on_melody_name(const char *name) { publish_string("pwon_melody", name); }
void mqtt_ha_publish_power_off_melody(uint8_t idx) {
    char b[4]; snprintf(b, sizeof(b), "%u", idx); publish_string("pwoff_melody", b);
}
void mqtt_ha_publish_power_off_melody_name(const char *name) { publish_string("pwoff_melody", name); }
void mqtt_ha_publish_alarm_melody_name(const char *name) { publish_string("alarm_melody", name); }
void mqtt_ha_publish_home_screen_name(const char *name) { publish_string("home_screen", name); }
void mqtt_ha_publish_sched_off_time(const char *t) { publish_string("sched_off_time", t ? t : ""); }
void mqtt_ha_publish_sched_on_time(const char *t)  { publish_string("sched_on_time", t ? t : ""); }
void mqtt_ha_publish_sched_off_day(uint8_t d) { char b[4]; snprintf(b, sizeof(b), "%u", d); publish_string("sched_off_day", b); }
void mqtt_ha_publish_sched_on_day(uint8_t d)  { char b[4]; snprintf(b, sizeof(b), "%u", d); publish_string("sched_on_day", b); }
void mqtt_ha_publish_sched_off_en(bool en) { publish_string("sched_off_en", en ? "ON" : "OFF"); }
void mqtt_ha_publish_sched_on_en(bool en)  { publish_string("sched_on_en", en ? "ON" : "OFF"); }
void mqtt_ha_publish_schedules_json(const char *json) { publish_string("schedules", json ? json : "[]"); }
