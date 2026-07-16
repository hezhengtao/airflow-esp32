#include "app_controller.h"
#include "board.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_sleep.h"

#include "factory_reset.h"
#include "settings.h"
#include "event_bus.h"
#include "motor_bldc.h"
#include "sensor_y01.h"
#include "sensor_ds18b20.h"
#include "mqtt_ha.h"
#include "ui/ui_theme.h"
#include "ui/ui_screen_power.h"
#include "ui/ui_screen_settings.h"
#include "wifi_prov.h"
#include "speaker.h"
#include "ui_manager.h"
#include "ui_boot.h"

#include "holiday/holiday_client.h"
#include "sensor_history.h"
#include "ui_provision_prompt.h"
#include "ui/ui_screen_network.h"
#include "status_led.h"
#include "lcd_tk043f1509.h"
#include "lwip/dns.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "app";

static app_state_t g_state = APP_STATE_INIT;
static app_settings_t g_settings;
static bool g_power_on = false;
static bool g_shutdown = false;  /* true after shutdown — blocks alarm/rpm tasks */
static volatile power_action_t g_pending_action = POWER_ACTION_NONE;
static bool g_wifi_ready = false;
static uint8_t g_fan_speed = 0;
static char g_wifi_ip[16] = {0};
static esp_lcd_panel_handle_t g_lcd_panel = NULL;

#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t g_wifi_event_group = NULL;
static int g_wifi_retry_count = 0;
#define WIFI_MAX_RETRIES     5
static esp_timer_handle_t g_wifi_bg_timer = NULL;
#define WIFI_BG_RETRY_SEC    300   /* 5-minute background retry */
static bool g_led_on = false;
static uint8_t g_led_r = 0, g_led_g = 255, g_led_b = 0;
static uint8_t g_led_brightness = 100;
static uint8_t g_led_cfg_state_id = 0;  /* currently selected state for HA per-state LED config */

/* Per-state LED configs: normal/alarm/shutdown/wifi_fail/wifi_connecting */
static led_state_cfg_t g_led_state[LED_STATE_COUNT] = {
    {  0, 255, 0, 2 },  /* normal: green, breathe */
    { 10,  0,  0, 0 },  /* alarm: red, steady */
    {  0,  0,  0, 0 },  /* shutdown: off */
    { 10,  2,  0, 0 },  /* WiFi fail: orange-red, steady */
    {  5,  3,  0, 0 },  /* WiFi connecting: yellow, steady */
    {  0,  0, 10, 1 },   /* OTA upgrade: blue, blink */
};

static void mqtt_init_from_settings(void);
static void status_led_update(void);

static bool g_mqtt_initialized = false;
static bool g_ota_in_progress = false;
static bool g_auto_started = false;

/* ── Latest sensor values for alarm monitor ─────────────────────────── */
static uint16_t g_latest_tvoc = 0;
static uint16_t g_latest_co2 = 0;
static uint16_t g_latest_ch2o = 0;
static uint32_t g_last_alarm_ts = 0;
static uint32_t g_boot_ts = 0;
static float g_indoor_temp = -999.0f;
#define AUTO_FAN_BOOT_DELAY_MS  10000

/* ── Background WiFi retry (after quick retries exhausted) ───────── */
static void wifi_bg_retry_cb(void *arg)
{
    ESP_LOGI(TAG, "Background WiFi retry...");
    g_wifi_retry_count = 0;  /* reset — start fresh quick-retry cycle */
    esp_wifi_connect();
}

/* ── WiFi event handler ──────────────────────────────────────────── */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected to AP, waiting for DHCP...");
        ui_update_wifi_status(true, g_settings.wifi_ssid);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason=%d, retry=%d",
                 disc->reason, g_wifi_retry_count);
        if (g_wifi_retry_count < WIFI_MAX_RETRIES) {
            g_wifi_retry_count++;
            esp_wifi_connect();
        } else {
            if (!g_wifi_bg_timer) {
                esp_timer_create_args_t bg_args = {
                    .callback = wifi_bg_retry_cb,
                    .name     = "wifi_bg",
                };
                esp_timer_create(&bg_args, &g_wifi_bg_timer);
                ESP_LOGI(TAG, "WiFi quick retries exhausted, starting background retry every %ds",
                         WIFI_BG_RETRY_SEC);
            }
            esp_timer_start_periodic(g_wifi_bg_timer,
                                     WIFI_BG_RETRY_SEC * 1000000ULL);
        }
        ui_update_wifi_status(false, NULL);
        g_mqtt_initialized = false;
        status_led_update();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip = event_data;
        g_wifi_retry_count = 0;
        if (g_wifi_bg_timer) {
            esp_timer_stop(g_wifi_bg_timer);
            ESP_LOGI(TAG, "WiFi reconnected, background retry stopped");
        }
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        event_t ev = { .id = EVENT_WIFI_CONNECTED };
        snprintf(ev.data.wifi.ssid, sizeof(ev.data.wifi.ssid), "%s",
                 g_settings.wifi_ssid);
        event_bus_publish(&ev);
        ui_update_wifi_status(true, g_settings.wifi_ssid);
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip->ip_info.ip));
        snprintf(g_wifi_ip, sizeof(g_wifi_ip), "%s", ip_str);
        ui_update_ip(ip_str);
        ui_provision_prompt_dismiss();  /* dismiss "connect WiFi" prompt if showing */
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&ip->ip_info.ip));
        /* Sync time via NTP (lwIP is ready now) */
        ui_theme_try_sntp();
        status_led_update();
        /* Start normal-mode HTTP server for device dashboard */
        wifi_prov_http_start_normal();
        /* Background WiFi scan — caches results so network screen is instant */
        ui_screen_network_boot_scan();
        /* Auto-start MQTT after WiFi connects (provisioned path) */
        if (!g_mqtt_initialized && g_settings.mqtt_uri[0]) {
            mqtt_init_from_settings();
        }
        /* Set DNS servers directly via LWIP — bypasses esp_netif config gaps */
        ip_addr_t dns1, dns2;
        ipaddr_aton("114.114.114.114", &dns1);
        ipaddr_aton("223.5.5.5", &dns2);
        dns_setserver(0, &dns1);
        dns_setserver(1, &dns2);
        ESP_LOGI(TAG, "DNS set: 114.114.114.114, 223.5.5.5");
        /* Start holiday now that network is ready */
        holiday_client_init();
    }
}

/* ── Provisioning state callback ─────────────────────────────────── */
static void on_prov_state(wifi_prov_state_t state, void *user_data)
{
    switch (state) {
    case WIFI_PROV_STATE_STARTED:
        ESP_LOGI(TAG, "WiFi provisioning started — AP: AiRFLOW");
        ui_update_wifi_status(false, "AiRFLOW (配网)");
        ui_update_ip("192.168.4.1");
        break;
    case WIFI_PROV_STATE_CRED_RECEIVED:
        ESP_LOGI(TAG, "Credentials received, connecting...");
        break;
    case WIFI_PROV_STATE_CONNECTED:
        ESP_LOGI(TAG, "Connected via provisioning");
        ui_update_wifi_status(true, g_settings.wifi_ssid);
        ui_provision_prompt_dismiss();  /* dismiss "connect WiFi" prompt */
        wifi_prov_stop();
        if (g_settings.mqtt_uri[0]) {
            mqtt_ha_start();
        }
        break;
    case WIFI_PROV_STATE_FAILED:
        ESP_LOGE(TAG, "Provisioning failed");
        break;
    default:
        break;
    }
}

/* ── STA mode: set config and start WiFi ─ */
static void wifi_init_sta(void)
{
    esp_wifi_set_mode(WIFI_MODE_STA);

    /* Enable all 2.4GHz protocols: b/g/n */
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                       WIFI_PROTOCOL_11N);

    if (g_settings.wifi_ssid[0]) {
        wifi_config_t wifi_cfg = {0};
        strncpy((char *)wifi_cfg.sta.ssid, g_settings.wifi_ssid,
                sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char *)wifi_cfg.sta.password, g_settings.wifi_pass,
                sizeof(wifi_cfg.sta.password) - 1);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    }
    esp_wifi_start();
}

/* ── MQTT ─────────────────────────────────────────────────────────── */
static void mqtt_command_handler(const char *topic, const char *payload,
                                  int payload_len, void *user_data);

static void mqtt_on_connected(void *user_data);
static void mqtt_on_disconnected(void *user_data);

static void mqtt_init_from_settings(void)
{
    if (!g_settings.mqtt_uri[0]) return;
    if (g_mqtt_initialized) return;
    size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "MQTT init: broker=%s free_dram=%u", g_settings.mqtt_uri, (unsigned)free_dram);
    mqtt_ha_init(g_settings.mqtt_uri,
                 g_settings.mqtt_user[0] ? g_settings.mqtt_user : NULL,
                 g_settings.mqtt_pass[0] ? g_settings.mqtt_pass : NULL,
                 g_settings.device_name, g_settings.device_id);
    mqtt_ha_set_command_callback(mqtt_command_handler, NULL);
    mqtt_ha_set_connected_callback(mqtt_on_connected, NULL);
    mqtt_ha_set_disconnected_callback(mqtt_on_disconnected, NULL);
    mqtt_ha_start();
    g_mqtt_initialized = true;
}

void app_controller_mqtt_apply(void)
{
    /* Reload from NVS into g_settings so the in-memory copy matches */
    settings_get_str(NVS_KEY_MQTT_URI, g_settings.mqtt_uri, sizeof(g_settings.mqtt_uri));
    settings_get_str(NVS_KEY_MQTT_USER, g_settings.mqtt_user, sizeof(g_settings.mqtt_user));
    settings_get_str(NVS_KEY_MQTT_PASS, g_settings.mqtt_pass, sizeof(g_settings.mqtt_pass));

    ESP_LOGI(TAG, "MQTT apply: broker=%s", g_settings.mqtt_uri);

    /* Stop current client and restart with new settings */
    mqtt_ha_stop();
    g_mqtt_initialized = false;

    if (g_settings.mqtt_uri[0]) {
        mqtt_init_from_settings();
    }
}

/* Melody and home-screen name arrays — must be before mqtt_command_handler */
static const char *g_key_melody_names[] = {"马里奥金币","短促滴答","水滴","电子哔","轻声滴答","双击"};
static const char *g_pwon_melody_names[] = {"马里奥主题","上行音阶","叮咚门铃","大三和弦","小星星","号角"};
static const char *g_pwoff_melody_names[] = {"下行GEC","长下行音阶","叮咚下行","小调琶音","再见","渐慢风落"};
static const char *g_alarm_melody_names[] = {"经典警笛","急促哔哔","高低扫频","三连脉冲","连续快滴","低频轰鸣"};
static const char *g_home_screen_names[] = {"首页","传感器","风扇"};

static bool topic_ends_with(const char *topic, const char *suffix)
{
    /* Match a full MQTT topic segment: the suffix must appear at the end
     * and be preceded by '/' (or be the entire topic).  This prevents
     * e.g. "fan_cmd" from matching "auto_fan_cmd". */
    size_t topic_len = strlen(topic);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > topic_len) return false;
    if (strcmp(topic + topic_len - suffix_len, suffix) != 0) return false;
    return (topic_len == suffix_len || topic[topic_len - suffix_len - 1] == '/');
}

static void mqtt_command_handler(const char *topic, const char *payload,
                                  int payload_len, void *user_data)
{
    if (topic_ends_with(topic, "fan_cmd")) {
        if (strncmp(payload, "ON", payload_len) == 0) {
            g_power_on = true;
            if (g_fan_speed < 5) { g_fan_speed = 50; motor_set_speed(50); }
            motor_start();
        } else {
            g_power_on = false; g_auto_started = false;  /* manual OFF → don't auto-restart */
            motor_stop();
        }
        mqtt_ha_publish_fan_state(g_power_on, g_fan_speed);
    } else if (topic_ends_with(topic, "fan_set_speed")) {
        int s = atoi(payload);
        if (s >= 0 && s <= 100) app_controller_set_fan_speed((uint8_t)s);
    } else if (topic_ends_with(topic, "fan_preset_cmd")) {
        int level = atoi(payload);
        uint8_t sp = 0;
        if (level == 1) sp = 20;
        else if (level == 2) sp = 40;
        else if (level == 3) sp = 60;
        else if (level == 4) sp = 80;
        else if (level == 5) sp = 100;
        if (sp > 0) app_controller_set_fan_speed(sp);
    } else if (topic_ends_with(topic, "auto_fan_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        app_controller_set_auto_fan(en);
        mqtt_ha_publish_auto_fan(en);
    } else if (topic_ends_with(topic, "key_sound_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        app_controller_set_key_sound(en);
    } else if (topic_ends_with(topic, "power_sound_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        app_controller_set_power_sound(en);
    } else if (topic_ends_with(topic, "key_volume_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 100) app_controller_set_key_volume((uint8_t)v);
    } else if (topic_ends_with(topic, "power_volume_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 100) app_controller_set_power_volume((uint8_t)v);
    } else if (topic_ends_with(topic, "alarm_sound_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        app_controller_set_alarm_sound(en);
    } else if (topic_ends_with(topic, "alarm_volume_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 100) app_controller_set_alarm_volume((uint8_t)v);
    } else if (topic_ends_with(topic, "alarm_cooldown_cmd")) {
        int v = atoi(payload);
        if (v >= 5 && v <= 600) app_controller_set_alarm_cooldown((uint16_t)v);
    } else if (topic_ends_with(topic, "key_melody_cmd")) {
        /* Accept both HA select option string and legacy integer */
        bool found = false;
        for (int i = 0; i <= MELODY_KEY_CLICK_MAX; i++) {
            if (strncmp(payload, g_key_melody_names[i], payload_len) == 0) {
                app_controller_set_key_melody((uint8_t)i); found = true; break;
            }
        }
        if (!found) { int v = atoi(payload); if (v >= 0 && v <= MELODY_KEY_CLICK_MAX) app_controller_set_key_melody((uint8_t)v); }
    } else if (topic_ends_with(topic, "pwon_melody_cmd")) {
        bool found = false;
        for (int i = 0; i <= MELODY_POWER_ON_MAX; i++) {
            if (strncmp(payload, g_pwon_melody_names[i], payload_len) == 0) {
                app_controller_set_power_on_melody((uint8_t)i); found = true; break;
            }
        }
        if (!found) { int v = atoi(payload); if (v >= 0 && v <= MELODY_POWER_ON_MAX) app_controller_set_power_on_melody((uint8_t)v); }
    } else if (topic_ends_with(topic, "pwoff_melody_cmd")) {
        bool found = false;
        for (int i = 0; i <= MELODY_POWER_OFF_MAX; i++) {
            if (strncmp(payload, g_pwoff_melody_names[i], payload_len) == 0) {
                app_controller_set_power_off_melody((uint8_t)i); found = true; break;
            }
        }
        if (!found) { int v = atoi(payload); if (v >= 0 && v <= MELODY_POWER_OFF_MAX) app_controller_set_power_off_melody((uint8_t)v); }
    } else if (topic_ends_with(topic, "alarm_melody_cmd")) {
        for (int i = 0; i <= MELODY_ALARM_MAX; i++) {
            if (strncmp(payload, g_alarm_melody_names[i], payload_len) == 0) {
                app_controller_set_alarm_melody((uint8_t)i); break;
            }
        }
    } else if (topic_ends_with(topic, "home_screen_cmd")) {
        for (int i = 0; i < 3; i++) {
            if (strncmp(payload, g_home_screen_names[i], payload_len) == 0) {
                settings_save_u32(NVS_KEY_HOME_SCREEN, (uint32_t)i); settings_commit();
                mqtt_ha_publish_home_screen_name(g_home_screen_names[i]);
                break;
            }
        }
    } else if (topic_ends_with(topic, "alarm_tvoc_cmd")) {
        uint16_t v = (uint16_t)atoi(payload);
        if (v >= 100) {
            g_settings.tvoc_alarm_ugm3 = v;
            settings_save(&g_settings);
            mqtt_ha_publish_tvoc_threshold(v);
        }
    } else if (topic_ends_with(topic, "alarm_co2_cmd")) {
        uint16_t v = (uint16_t)atoi(payload);
        if (v >= 400) {
            g_settings.co2_alarm_ppm = v;
            settings_save(&g_settings);
            mqtt_ha_publish_co2_threshold(v);
        }
    } else if (topic_ends_with(topic, "alarm_ch2o_cmd")) {
        uint16_t v = (uint16_t)atoi(payload);
        if (v >= 20) {
            g_settings.ch2o_alarm_ugm3 = v;
            settings_save(&g_settings);
            mqtt_ha_publish_ch2o_threshold(v);
        }
    } else if (topic_ends_with(topic, "restart_cmd")) {
        ESP_LOGW(TAG, "MQTT restart command received");
        mqtt_ha_stop();
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else if (topic_ends_with(topic, "screen_off_cmd")) {
        app_controller_request_power_action(POWER_ACTION_SCREEN_OFF);
    } else if (topic_ends_with(topic, "screen_on_cmd")) {
        app_controller_request_power_action(POWER_ACTION_WAKE);
    } else if (topic_ends_with(topic, "shutdown_cmd")) {
        app_controller_request_power_action(POWER_ACTION_SHUTDOWN);
    } else if (topic_ends_with(topic, "wake_cmd")) {
        app_controller_request_power_action(POWER_ACTION_WAKE);
    } else if (strstr(topic, "led_cmd")) {
        app_controller_set_status_led(strncmp(payload, "ON", payload_len) == 0);
    } else if (strstr(topic, "led_state_r_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, (uint8_t)v, c->g, c->b); }
    } else if (strstr(topic, "led_state_g_cmd")) {
        int v = atoi(payload); if (v > 255) v = (v * 255) / 1000;
        if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, c->r, (uint8_t)v, c->b); }
    } else if (strstr(topic, "led_state_b_cmd")) {
        int v = atoi(payload); if (v > 255) v = (v * 255) / 1000;
        if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, c->r, c->g, (uint8_t)v); }
    } else if (strstr(topic, "led_brightness_cmd") || strstr(topic, "brightness_cmd")) {
        int v = atoi(payload);
        if (strstr(topic, "led_brightness_cmd")) { if (v >= 1 && v <= 100) app_controller_set_led_brightness((uint8_t)v); }
        else { if (v >= 10 && v <= 100) app_controller_set_brightness((uint8_t)v); }
    } else if (strstr(topic, "led_cfg_state_cmd")) {
        static const char *n[] = {"正常","报警","关机","WiFi失败","WiFi连接中","OTA升级"};
        for (int i = 0; i < 6; i++) if (strncmp(payload, n[i], payload_len) == 0) { app_controller_set_led_cfg_state((uint8_t)i); break; }
    } else if (strstr(topic, "led_state_eff_cmd")) {
        static const char *n[] = {"常亮","慢闪","呼吸","快闪","彩虹"};
        for (int i = 0; i < 5; i++) if (strncmp(payload, n[i], payload_len) == 0) { app_controller_set_led_state_effect(g_led_cfg_state_id, (uint8_t)i); break; }
    } else if (strstr(topic, "led_state_r_cmd")) {
        int v = atoi(payload); ESP_LOGI(TAG, "led_state_r: %d (state=%d)", v, g_led_cfg_state_id);
        if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, (uint8_t)v, c->g, c->b); }
    } else if (strstr(topic, "led_state_g_cmd")) {
        int v = atoi(payload); if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, c->r, (uint8_t)v, c->b); }
    } else if (strstr(topic, "led_state_b_cmd")) {
        int v = atoi(payload); if (v >= 0 && v <= 255) { const led_state_cfg_t *c = app_controller_get_led_state_cfg(g_led_cfg_state_id); app_controller_set_led_state_rgb(g_led_cfg_state_id, c->r, c->g, (uint8_t)v); }
    }
    /* ── Schedule detail commands (HA → screen sync) ── */
    else if (topic_ends_with(topic, "sched_off_h_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 23) { settings_save_u32(NVS_KEY_SCHED_OFF_H, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    } else if (topic_ends_with(topic, "sched_off_m_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 59) { settings_save_u32(NVS_KEY_SCHED_OFF_M, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    } else if (topic_ends_with(topic, "sched_on_h_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 23) { settings_save_u32(NVS_KEY_SCHED_ON_H, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    } else if (topic_ends_with(topic, "sched_on_m_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 59) { settings_save_u32(NVS_KEY_SCHED_ON_M, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    } else if (topic_ends_with(topic, "sched_off_day_cmd")) {
        int v = atoi(payload);
        settings_save_u32(NVS_KEY_SCHED_OFF_DAY, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule();
    } else if (topic_ends_with(topic, "sched_on_day_cmd")) {
        int v = atoi(payload);
        settings_save_u32(NVS_KEY_SCHED_ON_DAY, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule();
    } else if (topic_ends_with(topic, "sched_off_mask_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 127) { settings_save_u32(NVS_KEY_SCHED_OFF_MASK, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    } else if (topic_ends_with(topic, "sched_on_mask_cmd")) {
        int v = atoi(payload);
        if (v >= 0 && v <= 127) { settings_save_u32(NVS_KEY_SCHED_ON_MASK, (uint32_t)v); settings_commit(); ui_screen_power_sync_schedule(); }
    }
    /* ── Schedule enable switches ── */
    else if (topic_ends_with(topic, "sched_off_en_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        settings_save_u32(NVS_KEY_SCHED_OFF_EN, en ? 1 : 0);
        settings_commit();
        ui_screen_power_sync_schedule();
    } else if (topic_ends_with(topic, "sched_on_en_cmd")) {
        bool en = (strncmp(payload, "ON", payload_len) == 0);
        settings_save_u32(NVS_KEY_SCHED_ON_EN, en ? 1 : 0);
        settings_commit();
        ui_screen_power_sync_schedule();
    }
    /* ── Schedule delete buttons ── */
    else if (topic_ends_with(topic, "sched_off_del_cmd")) {
        settings_save_u32(NVS_KEY_SCHED_OFF_EN, 0);
        settings_save_u8(NVS_KEY_SCHED_OFF_DAY, 3);  /* 每天 */
        settings_commit();
        ui_screen_power_sync_schedule();
        ESP_LOGI(TAG, "Off schedule deleted");
    } else if (topic_ends_with(topic, "sched_on_del_cmd")) {
        settings_save_u32(NVS_KEY_SCHED_ON_EN, 0);
        settings_save_u8(NVS_KEY_SCHED_ON_DAY, 3);  /* 每天 */
        settings_commit();
        ui_screen_power_sync_schedule();
        ESP_LOGI(TAG, "On schedule deleted");
    }
    /* ── Web multi-schedule JSON ── */
    else if (topic_ends_with(topic, "schedules_cmd")) {
        ui_screen_power_set_web_schedules(payload);
    }
}

/* ── Sensor callbacks ─────────────────────────────────────────────── */
static void on_y01_data(const y01_data_t *d, void *ctx)
{
    g_latest_tvoc = d->tvoc_ugm3;
    g_latest_co2 = d->co2_ppm;
    g_latest_ch2o = d->ch2o_ugm3;

    event_t ev = { .id = EVENT_SENSOR_Y01_UPDATE };
    ev.data.y01.tvoc = d->tvoc_ugm3;
    ev.data.y01.co2 = d->co2_ppm;
    ev.data.y01.ch2o = d->ch2o_ugm3;
    event_bus_publish(&ev);
    ui_update_tvoc(d->tvoc_ugm3);
    ui_update_co2(d->co2_ppm);
    ui_update_ch2o(d->ch2o_ugm3);
    mqtt_ha_publish_tvoc(d->tvoc_ugm3);
    mqtt_ha_publish_co2(d->co2_ppm);
    mqtt_ha_publish_ch2o(d->ch2o_ugm3);

    wifi_prov_sensor_t s = { .tvoc_ugm3 = d->tvoc_ugm3,
                              .co2_ppm = d->co2_ppm,
                              .ch2o_ugm3 = d->ch2o_ugm3 };
    wifi_prov_update_sensors(&s, WIFI_PROV_SENSOR_TVOC |
                                   WIFI_PROV_SENSOR_CO2 |
                                   WIFI_PROV_SENSOR_CH2O);

    /* Alarm check — runs on every sensor update instead of a dedicated task */
    if (!g_shutdown && g_settings.auto_fan_enable) {
        uint8_t alarm_type = 0;
        uint16_t alarm_val = 0, alarm_thr = 0;

        if (g_settings.tvoc_alarm_ugm3 > 0 && g_latest_tvoc >= g_settings.tvoc_alarm_ugm3) {
            alarm_type = ALARM_TYPE_TVOC;
            alarm_val = g_latest_tvoc;
            alarm_thr = g_settings.tvoc_alarm_ugm3;
        } else if (g_settings.co2_alarm_ppm > 0 && g_latest_co2 >= g_settings.co2_alarm_ppm) {
            alarm_type = ALARM_TYPE_CO2;
            alarm_val = g_latest_co2;
            alarm_thr = g_settings.co2_alarm_ppm;
        } else if (g_settings.ch2o_alarm_ugm3 > 0 && g_latest_ch2o >= g_settings.ch2o_alarm_ugm3) {
            alarm_type = ALARM_TYPE_CH2O;
            alarm_val = g_latest_ch2o;
            alarm_thr = g_settings.ch2o_alarm_ugm3;
        }

        if (alarm_type != 0) {
            ESP_LOGW(TAG, "Alarm! type=%d val=%u thr=%u", alarm_type, alarm_val, alarm_thr);
            status_led_update();

            if (!g_power_on) {
                g_power_on = true; g_auto_started = true;
                g_fan_speed = 70;
                motor_set_speed(70);
                motor_start();
                ui_update_fan_state(true);
                ui_update_fan_speed(70);
                mqtt_ha_publish_fan_state(true, 70);
            }

            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - g_last_alarm_ts > (uint32_t)g_settings.alarm_cooldown_s * 1000) {
                g_last_alarm_ts = now;
                speaker_alarm();
            }

            event_t ev = { .id = EVENT_ALARM_TRIGGERED };
            ev.data.alarm.type = alarm_type;
            ev.data.alarm.value = alarm_val;
            ev.data.alarm.threshold = alarm_thr;
            event_bus_publish(&ev);
            mqtt_ha_publish_alarm(alarm_type, alarm_val, alarm_thr);
        } else {
            /* Values below threshold → reset cooldown for next spike */
            g_last_alarm_ts = 0;
            /* Auto-stop: only if auto-started, with cooldown */
            static uint8_t y01_stop_cnt = 0;
            if (g_power_on && g_auto_started) {
                if (++y01_stop_cnt >= 10) { y01_stop_cnt = 0;
                g_power_on = false; g_auto_started = false;
                motor_stop();
                ui_update_fan_state(false);
                ESP_LOGI(TAG, "Auto-fan: air quality OK, motor stopped");
                }
            } else { y01_stop_cnt = 0; }
        }
    }
}

static void on_temp_data(float temp_c, void *ctx)
{
    /* Ignore invalid readings when sensor is disconnected.
     * PT1000 open-circuit typically reads very low (< -50°C) or very high. */
    if (temp_c < -50.0f || temp_c > 150.0f) return;

    g_indoor_temp = temp_c;
    ESP_LOGD("temp", "temp_cb=%.1f", temp_c);

    event_t ev = { .id = EVENT_SENSOR_TEMP_UPDATE };
    ev.data.temp.temp_c = temp_c;
    event_bus_publish(&ev);
    ui_update_temperature(temp_c);
    mqtt_ha_publish_temperature(temp_c);

    wifi_prov_sensor_t s = { .temp_c = temp_c };
    wifi_prov_update_sensors(&s, WIFI_PROV_SENSOR_TEMP);
}

/* 10-second periodic timer — replaces periodic_monitor_task + alarm_monitor_task.
 * FreeRTOS timer callbacks run with minimal stack in the timer task context. */
static void periodic_10s_cb(TimerHandle_t xTimer)
{
    /* Fan state to MQTT */
    mqtt_ha_publish_fan_state(g_power_on, g_fan_speed);
    mqtt_ha_publish_fan_preset(g_power_on ? g_fan_speed : 0);

    /* Power + schedule binary sensors */
    mqtt_ha_publish_power_state(!g_shutdown);
    mqtt_ha_publish_schedule_active(ui_screen_power_is_schedule_active());

    /* Status LED (alarm may have cleared) */
    status_led_update();

    /* WiFi info + uptime */
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);
    const char *ssid = g_settings.wifi_ssid[0] ? g_settings.wifi_ssid : NULL;
    const char *ip = g_wifi_ip[0] ? g_wifi_ip : NULL;
    mqtt_ha_publish_wifi_info(rssi, ssid, ip);
    uint32_t uptime = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
    mqtt_ha_publish_uptime(uptime);

    uint16_t rpm = (!g_shutdown && g_power_on) ? motor_get_rpm() : 0;

    /* RPM to MQTT (HA sensor) */
    mqtt_ha_publish_fan_rpm(rpm);

    /* RPM to web dashboard */
    wifi_prov_sensor_t s = {
        .fan_rpm = rpm,
        .fan_on = (!g_shutdown && g_power_on),
        .fan_speed = g_fan_speed
    };
    wifi_prov_update_sensors(&s, WIFI_PROV_SENSOR_FAN);

    /* No-motor detection: if FG=0 for 30s while fan is ON, auto-stop */
    static uint8_t no_motor_cnt = 0;
    if (g_power_on && motor_get_rpm() == 0) {
        if (++no_motor_cnt >= 5) {  /* 5×2s = 10s */
            g_power_on = false; g_auto_started = false; motor_stop();
            ui_update_fan_state(false);
            ESP_LOGI(TAG, "Auto-fan: no motor detected, stopped");
        }
    } else { no_motor_cnt = 0; }

    /* Auto-fan alarm check (fallback — also in on_y01_data callback) */
    static uint8_t auto_stop_cnt = 0;
    static bool logged_af = false;
    if (!logged_af) { ESP_LOGI(TAG, "Auto-fan: enabled=%d tvoc=%u/%u co2=%u/%u", g_settings.auto_fan_enable, g_latest_tvoc, g_settings.tvoc_alarm_ugm3, g_latest_co2, g_settings.co2_alarm_ppm); logged_af = true; }
    if (!g_shutdown && g_settings.auto_fan_enable) {
        uint32_t upt = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (upt - g_boot_ts >= AUTO_FAN_BOOT_DELAY_MS) {
        bool over = false;
        if (g_settings.tvoc_alarm_ugm3 > 0 && g_latest_tvoc >= g_settings.tvoc_alarm_ugm3) over = true;
        if (g_settings.co2_alarm_ppm  > 0 && g_latest_co2  >= g_settings.co2_alarm_ppm)  over = true;
        if (g_settings.ch2o_alarm_ugm3 > 0 && g_latest_ch2o >= g_settings.ch2o_alarm_ugm3) over = true;
        if (over) {
            auto_stop_cnt = 0;
            if (!g_power_on) {
                g_power_on = true; g_fan_speed = 70; g_auto_started = true;
                motor_set_speed(70); motor_start();
                ui_update_fan_state(true); ui_update_fan_speed(70);
                ESP_LOGI(TAG, "Auto-fan: alarm triggered, motor ON 70%%");
            }
        } else if (g_power_on && g_auto_started) {
            if (++auto_stop_cnt >= 5) {  /* 5×2s = 10s below threshold → stop */
                auto_stop_cnt = 0;
                g_power_on = false; g_auto_started = false; motor_stop();
                ui_update_fan_state(false);
                ESP_LOGI(TAG, "Auto-fan: air OK, motor stopped");
            }
        }
        } /* end if (upt >= boot delay) */
    } else {
        auto_stop_cnt = 0;
    }

    /* Holiday name (once per hour) */
    {
        static uint32_t last_holiday_s = 0;
        uint32_t now_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        if (now_s - last_holiday_s >= 3600) {
            last_holiday_s = now_s;
            time_t now = time(NULL);
            struct tm ti;
            localtime_r(&now, &ti);
            if (ti.tm_year + 1900 >= 2025) {
                const char *holiday = holiday_get_name(ti.tm_mon + 1, ti.tm_mday);
                mqtt_ha_publish_holiday(holiday ? holiday : "");
            }
        }
    }
}

/* 1-second RPM UI update — fast enough for real-time display */
static void rpm_ui_1s_cb(TimerHandle_t xTimer)
{
    if (!g_shutdown && g_power_on) {
        ui_update_fan_rpm(motor_get_rpm());
    }
}

/* 60-second sensor history sampler */
static void history_60s_cb(TimerHandle_t xTimer)
{
    time_t now = time(NULL);
    if (now < 1000000000) return;  /* SNTP not synced yet */

    float temp = g_indoor_temp;
    if (temp < -50.0f) temp = -999.0f;

    sensor_history_add(now, temp, g_latest_tvoc, g_latest_co2, g_latest_ch2o);
}

/* ── Boot confirm timer ───────────────────────────────────────────── */
static void boot_confirm_timer_cb(TimerHandle_t t)
{
    factory_reset_confirm_boot();
}

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC API
   ═══════════════════════════════════════════════════════════════════ */

void app_controller_init(void)
{
    ESP_LOGI(TAG, "══════════════════════════════════════════");
    ESP_LOGI(TAG, "  AiRFLOW Smart Air Purifier  v2.0");
    ESP_LOGI(TAG, "  ESP32-S3 + TK043F1509 Display");
    ESP_LOGI(TAG, "══════════════════════════════════════════");

    /* 1. NVS */
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* 2. Factory reset check */
    if (factory_reset_check()) {
        ESP_LOGW(TAG, "Factory reset triggered — restarting");
        esp_restart();
    }

    /* 3. Settings */
    settings_init();
    settings_load(&g_settings);

    /* Apply loaded sound settings */
    speaker_set_volume(g_settings.key_volume);
    speaker_set_click_enabled(g_settings.key_sound_enable);
    speaker_set_power_volume(g_settings.power_volume);
    speaker_set_power_sound_enabled(g_settings.power_sound_enable);
    speaker_set_alarm_volume(g_settings.alarm_volume);
    speaker_set_alarm_sound_enabled(g_settings.alarm_sound_enable);
    speaker_set_key_melody(g_settings.key_melody);
    speaker_set_power_on_melody(g_settings.power_on_melody);
    speaker_set_power_off_melody(g_settings.power_off_melody);
    speaker_set_alarm_melody(g_settings.alarm_melody);

    /* 4. Event bus */
    event_bus_init();

    /* ── WiFi low-level init BEFORE display to ensure heap is available ── */
    ESP_LOGI(TAG, "[1/5] WiFi low-level init...");
    esp_netif_init();
    esp_event_loop_create_default();
    g_wifi_event_group = xEventGroupCreate();

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta_netif, "AiRFLOW");
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t wifi_err = esp_wifi_init(&wifi_cfg);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init FAILED: %s (0x%X)", esp_err_to_name(wifi_err), wifi_err);
    } else {
        ESP_LOGI(TAG, "esp_wifi_init OK");
        g_wifi_ready = true;
    }
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    esp_log_level_set("FT5x06", ESP_LOG_ERROR);

    /* Init status LED early — WiFi events may call status_led_update()
     * before the main peripheral init loop runs.  RMT must be ready. */
    status_led_init();

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         wifi_event_handler, NULL, NULL);

    /* ── Display (user sees boot animation while WiFi connects) ── */
    ESP_LOGI(TAG, "[2/5] LCD init...");
    esp_lcd_panel_io_handle_t lcd_io = NULL;
    esp_lcd_panel_handle_t lcd_panel = NULL;
    lcd_init(&lcd_io, &lcd_panel);
    g_lcd_panel = lcd_panel;

    ESP_LOGI(TAG, "[3/5] Speaker init...");
    speaker_init();

    /* ── Peripherals (before UI — ADC/DRAM hungry) ─────────────────── */
    ESP_LOGI(TAG, "[4/5] Starting peripherals...");
    motor_init();
    y01_init();
    y01_set_callback(on_y01_data, NULL);
    ds18b20_init();
    ds18b20_set_callback(on_temp_data, NULL);
    sensor_history_init();

    /* ── UI init (LVGL + Touch) ────────────────────────────────────── */
    ESP_LOGI(TAG, "[5/5] UI init (LVGL + Touch)...");
    ui_manager_init(lcd_io, lcd_panel);
    lvgl_port_lock(1000);
    ui_boot_show(lv_screen_active(), ui_manager_show);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* ── WiFi high-level: start connection or provisioning ── */
    wifi_prov_init();
    wifi_prov_set_state_callback(on_prov_state, NULL);

    if (!wifi_prov_is_provisioned()) {
        ESP_LOGI(TAG, "Starting provisioning SoftAP...");
        wifi_prov_start();
    } else {
        ESP_LOGI(TAG, "Connecting to stored WiFi...");
        wifi_init_sta();
        EventBits_t bits = 0;
        for (int i = 0; i < 100; i++) {
            bits = xEventGroupWaitBits(g_wifi_event_group, WIFI_CONNECTED_BIT,
                                        pdFALSE, pdFALSE, pdMS_TO_TICKS(100));
            if (bits & WIFI_CONNECTED_BIT) break;
        }
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi connected");
        } else {
            ESP_LOGW(TAG, "Stored WiFi failed — starting provisioning AP");
            wifi_prov_start();
        }
    }

    if (!wifi_prov_is_provisioned()) {
        ESP_LOGI(TAG, "Waiting for provisioning (connect to AiRFLOW)...");
    } else {
        ESP_LOGI(TAG, "Running...");
    }

    /* 2-second periodic timer (fan, LED, WiFi info, uptime) */
    TimerHandle_t pt = xTimerCreate("per2s", pdMS_TO_TICKS(2000), pdTRUE, NULL, periodic_10s_cb);
    if (pt) xTimerStart(pt, 0);

    /* 1-second RPM UI refresh timer */
    TimerHandle_t rt = xTimerCreate("rpm_ui", pdMS_TO_TICKS(1000), pdTRUE, NULL, rpm_ui_1s_cb);
    if (rt) xTimerStart(rt, 0);

    /* 60-second history sampler */
    TimerHandle_t ht = xTimerCreate("hist60s", pdMS_TO_TICKS(60000), pdTRUE, NULL, history_60s_cb);
    if (ht) {
        xTimerStart(ht, 0);
    } else {
        ESP_LOGE(TAG, "FATAL: history timer create failed");
    }

    /* Init status LED — load saved state, then update based on current status */
    status_led_init();
    uint32_t led_on = 1, val = 0;
    settings_get_u32(NVS_KEY_LED_ON, &led_on);
    g_led_on = (led_on != 0);
    if (settings_get_u32(NVS_KEY_LED_R, &val) == ESP_OK) g_led_r = (uint8_t)val;
    if (settings_get_u32(NVS_KEY_LED_G, &val) == ESP_OK) g_led_g = (uint8_t)val;
    if (settings_get_u32(NVS_KEY_LED_B, &val) == ESP_OK) g_led_b = (uint8_t)val;
    if (settings_get_u32(NVS_KEY_LED_BRIGHT, &val) == ESP_OK) g_led_brightness = (uint8_t)val;

    /* Load per-state LED configs from NVS */
    static const char *state_keys[LED_STATE_COUNT][4] = {
        {NVS_KEY_LED_N_R, NVS_KEY_LED_N_G, NVS_KEY_LED_N_B, NVS_KEY_LED_N_EFF},
        {NVS_KEY_LED_A_R, NVS_KEY_LED_A_G, NVS_KEY_LED_A_B, NVS_KEY_LED_A_EFF},
        {NVS_KEY_LED_S_R, NVS_KEY_LED_S_G, NVS_KEY_LED_S_B, NVS_KEY_LED_S_EFF},
        {NVS_KEY_LED_F_R, NVS_KEY_LED_F_G, NVS_KEY_LED_F_B, NVS_KEY_LED_F_EFF},
        {NVS_KEY_LED_C_R, NVS_KEY_LED_C_G, NVS_KEY_LED_C_B, NVS_KEY_LED_C_EFF},
        {NVS_KEY_LED_O_R, NVS_KEY_LED_O_G, NVS_KEY_LED_O_B, NVS_KEY_LED_O_EFF},
    };
    for (int i = 0; i < LED_STATE_COUNT; i++) {
        uint8_t v8;
        if (settings_get_u8(state_keys[i][0], &v8) == ESP_OK) g_led_state[i].r = v8;
        if (settings_get_u8(state_keys[i][1], &v8) == ESP_OK) g_led_state[i].g = v8;
        if (settings_get_u8(state_keys[i][2], &v8) == ESP_OK) g_led_state[i].b = v8;
        if (settings_get_u8(state_keys[i][3], &v8) == ESP_OK) g_led_state[i].effect = v8;
    }
    status_led_update();

    TimerHandle_t bt = xTimerCreate("boot_ok", pdMS_TO_TICKS(35000),
                                     pdFALSE, NULL, boot_confirm_timer_cb);
    xTimerStart(bt, 0);

    g_boot_ts = xTaskGetTickCount() * portTICK_PERIOD_MS;
    g_state = APP_STATE_RUNNING;
    ESP_LOGI(TAG, "Ready.");
    speaker_power_on();
}

void app_controller_start(void)
{
    uint32_t last_sensor = 0;
    while (1) {
        /* lv_timer_handler() is handled by LVGL port task — do NOT call it here */
        vTaskDelay(pdMS_TO_TICKS(5));

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_sensor >= 2000) {
            ds18b20_read();
            last_sensor = now;
        }
    }
}

app_state_t app_controller_get_state(void) { return g_state; }

void app_controller_toggle_power(void)
{
    g_auto_started = false;
    g_power_on = !g_power_on;
    if (g_power_on) {
        speaker_power_on();
        if (g_fan_speed < 5) { g_fan_speed = 50; motor_set_speed(50); }
        motor_start();
    } else {
        speaker_power_off();
        motor_stop();
    }
    ui_update_fan_state(g_power_on);
    mqtt_ha_publish_fan_state(g_power_on, g_fan_speed);

    wifi_prov_sensor_t s = { .fan_on = g_power_on, .fan_speed = g_fan_speed };
    wifi_prov_update_sensors(&s, WIFI_PROV_SENSOR_FAN);
}

void app_controller_set_fan_speed(uint8_t pct)
{
    g_auto_started = false;  /* manual speed change → user in control */
    if (g_fan_speed == 0 && pct > 0) pct = 50;
    g_fan_speed = pct;

    if (pct > 0) {
        /* Map 1→5, 100→100 linearly */
        uint8_t motor_pct = (uint8_t)(10 + ((uint32_t)(pct - 1) * 90) / 99);
        motor_set_speed(motor_pct);
        if (!g_power_on) {
            g_power_on = true;
            motor_start();
            ui_update_fan_state(true);
        }
    } else {
        if (g_power_on) {
            g_power_on = false; g_auto_started = false;
            motor_stop();
            ui_update_fan_state(false);
        }
    }

    ui_update_fan_speed(pct);
    mqtt_ha_publish_fan_state(g_power_on, g_fan_speed);

    wifi_prov_sensor_t s = { .fan_speed = g_fan_speed };
    wifi_prov_update_sensors(&s, WIFI_PROV_SENSOR_FAN);
}

void app_controller_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset initiated");
    settings_erase_all();
    wifi_prov_erase_config();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void app_controller_wifi_scan(void)
{
    ESP_LOGI(TAG, "WiFi scan triggered");
    /* AP scan would go here — populate UI list */
}

void app_controller_wifi_connect(const char *ssid, const char *pass)
{
    if (ssid) strncpy(g_settings.wifi_ssid, ssid, sizeof(g_settings.wifi_ssid) - 1);
    if (pass) strncpy(g_settings.wifi_pass, pass, sizeof(g_settings.wifi_pass) - 1);
    settings_save(&g_settings);

    /* Block auto-retry in the disconnect handler so it doesn't race with
     * our new config — otherwise the old credentials reconnect first. */
    g_wifi_retry_count = WIFI_MAX_RETRIES;

    /* Disconnect from current AP, then apply new config */
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(300));

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, g_settings.wifi_ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, g_settings.wifi_pass, sizeof(cfg.sta.password) - 1);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set_config failed: %s", esp_err_to_name(err));
    }

    g_wifi_retry_count = 0;
    err = esp_wifi_connect();
    ESP_LOGI(TAG, "WiFi connect requested: SSID=%s err=%s",
             g_settings.wifi_ssid, esp_err_to_name(err));
}

/* ── Start provisioning manually (from WiFi screen) ───────────────── */
void app_controller_start_provisioning(void)
{
    wifi_prov_start();
}

/* ── Reset provisioning data ──────────────────────────────────────── */
void app_controller_reset_provisioning(void)
{
    wifi_prov_erase_config();
    esp_restart();
}

/* ── Alarm threshold management ─────────────────────────────────────── */

void app_controller_set_alarm_threshold(uint16_t tvoc_ugm3, uint16_t co2_ppm, uint16_t ch2o_ugm3)
{
    g_settings.tvoc_alarm_ugm3 = tvoc_ugm3;
    g_settings.co2_alarm_ppm = co2_ppm;
    g_settings.ch2o_alarm_ugm3 = ch2o_ugm3;
    settings_save(&g_settings);
    mqtt_ha_publish_tvoc_threshold(tvoc_ugm3);
    mqtt_ha_publish_co2_threshold(co2_ppm);
    mqtt_ha_publish_ch2o_threshold(ch2o_ugm3);
    ESP_LOGI(TAG, "Alarm thresholds: TVOC=%u CO2=%u CH2O=%u",
             tvoc_ugm3, co2_ppm, ch2o_ugm3);
}

void app_controller_set_auto_fan(bool enable)
{
    g_settings.auto_fan_enable = enable;
    settings_save(&g_settings);
    mqtt_ha_publish_auto_fan(enable);
    ESP_LOGI(TAG, "Auto fan: %s", enable ? "ON" : "OFF");
    if (!enable && g_auto_started) {
        /* Turn off fan if it was auto-started and auto-fan is disabled */
        g_power_on = false; g_auto_started = false;
        motor_stop();
        ui_update_fan_state(false);
        mqtt_ha_publish_fan_state(false, g_fan_speed);
    }
    if (enable) {
        /* Check immediately: start fan if thresholds exceeded */
        bool over = false;
        if (g_settings.tvoc_alarm_ugm3 > 0 && g_latest_tvoc >= g_settings.tvoc_alarm_ugm3) over = true;
        if (g_settings.co2_alarm_ppm  > 0 && g_latest_co2  >= g_settings.co2_alarm_ppm)  over = true;
        if (g_settings.ch2o_alarm_ugm3 > 0 && g_latest_ch2o >= g_settings.ch2o_alarm_ugm3) over = true;
        if (over && !g_power_on) {
            g_power_on = true; g_auto_started = true;
            g_fan_speed = 70; motor_set_speed(70); motor_start();
            ui_update_fan_state(true); ui_update_fan_speed(70);
            mqtt_ha_publish_fan_state(true, 70);
        }
    }
}

bool app_controller_get_auto_fan(void)
{
    return g_settings.auto_fan_enable;
}

void app_controller_get_alarm_thresholds(uint16_t *tvoc, uint16_t *co2, uint16_t *ch2o)
{
    if (tvoc) *tvoc = g_settings.tvoc_alarm_ugm3;
    if (co2)  *co2  = g_settings.co2_alarm_ppm;
    if (ch2o) *ch2o = g_settings.ch2o_alarm_ugm3;
}

/* ── MQTT connected callback — re-publish all state ───────────────── */

static void mqtt_on_connected(void *user_data)
{
    ui_update_mqtt_status(true);
    /* Fan */
    mqtt_ha_publish_fan_state(g_power_on, g_fan_speed);
    mqtt_ha_publish_fan_preset(g_power_on ? g_fan_speed : 0);
    /* Power + schedule */
    mqtt_ha_publish_power_state(!g_shutdown);
    mqtt_ha_publish_schedule_active(ui_screen_power_is_schedule_active());
    ui_screen_power_publish_schedule();
    /* Sensors: temperature (latest values) */
    mqtt_ha_publish_auto_fan(g_settings.auto_fan_enable);
    /* Sound */
    mqtt_ha_publish_key_sound(g_settings.key_sound_enable);
    mqtt_ha_publish_power_sound(g_settings.power_sound_enable);
    mqtt_ha_publish_key_volume(g_settings.key_volume);
    mqtt_ha_publish_power_volume(g_settings.power_volume);
    mqtt_ha_publish_alarm_sound(g_settings.alarm_sound_enable);
    mqtt_ha_publish_alarm_volume(g_settings.alarm_volume);
    mqtt_ha_publish_alarm_cooldown(g_settings.alarm_cooldown_s);
    mqtt_ha_publish_key_melody(g_settings.key_melody);
    mqtt_ha_publish_key_melody_name(g_key_melody_names[g_settings.key_melody]);
    mqtt_ha_publish_power_on_melody(g_settings.power_on_melody);
    mqtt_ha_publish_power_on_melody_name(g_pwon_melody_names[g_settings.power_on_melody]);
    mqtt_ha_publish_power_off_melody(g_settings.power_off_melody);
    mqtt_ha_publish_power_off_melody_name(g_pwoff_melody_names[g_settings.power_off_melody]);
    mqtt_ha_publish_alarm_melody_name(g_alarm_melody_names[g_settings.alarm_melody]);
    mqtt_ha_publish_home_screen_name(g_home_screen_names[g_settings.home_screen]);
    /* Brightness */
    mqtt_ha_publish_brightness(g_settings.brightness);
    /* Alarm thresholds */
    mqtt_ha_publish_tvoc_threshold(g_settings.tvoc_alarm_ugm3);
    mqtt_ha_publish_co2_threshold(g_settings.co2_alarm_ppm);
    mqtt_ha_publish_ch2o_threshold(g_settings.ch2o_alarm_ugm3);
    /* WiFi */
    int rssi = 0;
    esp_wifi_sta_get_rssi(&rssi);
    mqtt_ha_publish_wifi_info(rssi,
        g_settings.wifi_ssid[0] ? g_settings.wifi_ssid : NULL,
        g_wifi_ip[0] ? g_wifi_ip : NULL);
    /* Indoor temperature — publish valid value, or clear stale retained */
    if (g_indoor_temp > -50.0f && g_indoor_temp < 150.0f) {
        mqtt_ha_publish_temperature(g_indoor_temp);
    } else {
        mqtt_ha_publish_clear("temperature");
    }
    /* Y01 air quality — republish latest values (may have arrived before MQTT connected) */
    mqtt_ha_publish_tvoc(g_latest_tvoc);
    mqtt_ha_publish_co2(g_latest_co2);
    mqtt_ha_publish_ch2o(g_latest_ch2o);
    ESP_LOGI(TAG, "MQTT state synced after connect");
}

static void mqtt_on_disconnected(void *user_data)
{
    ui_update_mqtt_status(false);
}

/* ── Sound / brightness setters (persist + MQTT) ──────────────────── */

void app_controller_set_brightness(uint8_t pct)
{
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    g_settings.brightness = pct;
    settings_save(&g_settings);
    lcd_set_backlight(pct);
    mqtt_ha_publish_brightness(pct);
    ui_settings_update_brightness(pct);
}

uint8_t app_controller_get_brightness(void) { return g_settings.brightness; }

void app_controller_set_key_sound(bool enabled)
{
    speaker_set_click_enabled(enabled);
    g_settings.key_sound_enable = enabled;
    settings_save(&g_settings);
    mqtt_ha_publish_key_sound(enabled);
}

void app_controller_set_power_sound(bool enabled)
{
    speaker_set_power_sound_enabled(enabled);
    g_settings.power_sound_enable = enabled;
    settings_save(&g_settings);
    mqtt_ha_publish_power_sound(enabled);
}

void app_controller_set_key_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    speaker_set_volume(pct);
    g_settings.key_volume = pct;
    settings_save(&g_settings);
    mqtt_ha_publish_key_volume(pct);
}

void app_controller_set_power_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    speaker_set_power_volume(pct);
    g_settings.power_volume = pct;
    settings_save(&g_settings);
    mqtt_ha_publish_power_volume(pct);
}

bool app_controller_get_key_sound(void) { return g_settings.key_sound_enable; }
bool app_controller_get_power_sound(void) { return g_settings.power_sound_enable; }
uint8_t app_controller_get_key_volume(void) { return g_settings.key_volume; }
uint8_t app_controller_get_power_volume(void) { return g_settings.power_volume; }

void app_controller_set_alarm_sound(bool enabled)
{
    speaker_set_alarm_sound_enabled(enabled);
    g_settings.alarm_sound_enable = enabled;
    settings_save(&g_settings);
    mqtt_ha_publish_alarm_sound(enabled);
}

bool app_controller_get_alarm_sound(void) { return g_settings.alarm_sound_enable; }

void app_controller_set_alarm_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    speaker_set_alarm_volume(pct);
    g_settings.alarm_volume = pct;
    settings_save(&g_settings);
    mqtt_ha_publish_alarm_volume(pct);
}

uint8_t app_controller_get_alarm_volume(void) { return g_settings.alarm_volume; }

void app_controller_set_alarm_cooldown(uint16_t seconds) {
    if (seconds < 5) seconds = 5;
    if (seconds > 600) seconds = 600;
    g_settings.alarm_cooldown_s = seconds;
    settings_save(&g_settings);
    mqtt_ha_publish_alarm_cooldown(seconds);
}
uint16_t app_controller_get_alarm_cooldown(void) { return g_settings.alarm_cooldown_s; }


void app_controller_set_key_melody(uint8_t idx)
{
    if (idx > MELODY_KEY_CLICK_MAX) idx = 0;
    speaker_set_key_melody(idx);
    g_settings.key_melody = idx;
    settings_save(&g_settings);
    mqtt_ha_publish_key_melody_name(g_key_melody_names[idx]);
    mqtt_ha_publish_key_melody(idx);
}

void app_controller_set_power_on_melody(uint8_t idx)
{
    if (idx > MELODY_POWER_ON_MAX) idx = 0;
    speaker_set_power_on_melody(idx);
    g_settings.power_on_melody = idx;
    settings_save(&g_settings);
    mqtt_ha_publish_power_on_melody_name(g_pwon_melody_names[idx]);
    mqtt_ha_publish_power_on_melody(idx);
}

void app_controller_set_power_off_melody(uint8_t idx)
{
    if (idx > MELODY_POWER_OFF_MAX) idx = 0;
    speaker_set_power_off_melody(idx);
    g_settings.power_off_melody = idx;
    settings_save(&g_settings);
    mqtt_ha_publish_power_off_melody_name(g_pwoff_melody_names[idx]);
    mqtt_ha_publish_power_off_melody(idx);
}

uint8_t app_controller_get_key_melody(void) { return g_settings.key_melody; }
uint8_t app_controller_get_power_on_melody(void) { return g_settings.power_on_melody; }
uint8_t app_controller_get_power_off_melody(void) { return g_settings.power_off_melody; }

void app_controller_preview_key_melody(uint8_t idx) { speaker_click_by_idx(idx); }
void app_controller_preview_power_on_melody(uint8_t idx) { speaker_power_on_by_idx(idx); }
void app_controller_preview_power_off_melody(uint8_t idx) { speaker_power_off_by_idx(idx); }
void app_controller_set_alarm_melody(uint8_t idx) {
    if (idx > MELODY_ALARM_MAX) idx = 0;
    speaker_set_alarm_melody(idx);
    g_settings.alarm_melody = idx;
    settings_save(&g_settings);
    mqtt_ha_publish_alarm_melody_name(g_alarm_melody_names[idx]);
}
uint8_t app_controller_get_alarm_melody(void) { return speaker_get_alarm_melody(); }
void app_controller_preview_alarm_melody(uint8_t idx) { speaker_alarm_preview(idx); }

/* ── Screen off (backlight off, LCD panel stays awake for LVGL) ──── */
void app_controller_screen_off(void)
{
    ESP_LOGI(TAG, "Screen off — backlight off, panel alive");
    lcd_set_backlight(0);
}

void app_controller_screen_on(void)
{
    ESP_LOGI(TAG, "Screen on — backlight restored");
    lcd_set_backlight(g_settings.brightness > 0 ? g_settings.brightness : 80);
}

/* ── Soft shutdown (stop fan+sensors, black overlay, LCD panel stays alive) ── */
void app_controller_shutdown(void)
{
    ESP_LOGI(TAG, "Soft shutdown — fan off, sensors off");

    g_shutdown = true;

    if (g_power_on) {
        motor_stop();
        g_power_on = false;
        ui_update_fan_state(false);
        mqtt_ha_publish_fan_state(false, 0);
    }

    speaker_power_off();
    status_led_update();
    ESP_LOGI(TAG, "Ready for double-tap wake");
}

void app_controller_wake(void)
{
    g_shutdown = false;
    status_led_update();
    ESP_LOGI(TAG, "Wake from shutdown");
}

void app_controller_request_power_action(power_action_t action)
{
    g_pending_action = action;
}

power_action_t app_controller_consume_power_action(void)
{
    power_action_t a = g_pending_action;
    g_pending_action = POWER_ACTION_NONE;
    return a;
}

/* ── Status LED state-based color ──────────────────────────────────── */

static void status_led_update(void)
{
    if (g_shutdown) {
        status_led_set_on(false);
        return;
    }

    if (!g_led_on) {
        status_led_set_on(false);
        return;
    }

    /* Determine which state we're in */
    led_state_id_t state = LED_STATE_NORMAL;

    bool alarm = false;
    if (g_settings.tvoc_alarm_ugm3 > 0 && g_latest_tvoc >= g_settings.tvoc_alarm_ugm3) alarm = true;
    if (g_settings.co2_alarm_ppm  > 0 && g_latest_co2  >= g_settings.co2_alarm_ppm)  alarm = true;
    if (g_settings.ch2o_alarm_ugm3 > 0 && g_latest_ch2o >= g_settings.ch2o_alarm_ugm3) alarm = true;

    if (alarm) {
        state = LED_STATE_ALARM;
    } else if (g_shutdown) {
        state = LED_STATE_SHUTDOWN;
    } else if (g_ota_in_progress) {
        state = LED_STATE_OTA_UPGRADE;
    } else if (g_wifi_retry_count >= WIFI_MAX_RETRIES) {
        state = LED_STATE_WIFI_FAIL;
    } else {
        EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            state = LED_STATE_WIFI_CONNECTING;
        }
    }

    led_state_cfg_t *cfg = &g_led_state[state];

    /* Shutdown with black = fully off */
    if (state == LED_STATE_SHUTDOWN && cfg->r == 0 && cfg->g == 0 && cfg->b == 0) {
        status_led_set_on(false);
        return;
    }

    uint8_t r, g, b, bri;
    r = cfg->r; g = cfg->g; b = cfg->b;
    bri = g_led_brightness;

    ESP_LOGD(TAG, "LED apply: state=%d RGB=(%d,%d,%d) eff=%d bri=%d",
             (int)state, r, g, b, cfg->effect, bri);
    status_led_set_rgb(r, g, b);
    status_led_set_brightness(bri);
    status_led_set_effect((led_effect_t)cfg->effect);
    status_led_set_on(true);
}

void app_controller_set_status_led(bool on)
{
    g_led_on = on;
    status_led_update();
    settings_save_u32(NVS_KEY_LED_ON, on ? 1 : 0);
    settings_commit();

}

void app_controller_set_led_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    g_led_r = r; g_led_g = g; g_led_b = b;
    /* Sync to per-state configs so the change is visible in every state */
    g_led_state[LED_STATE_NORMAL].r = r;  g_led_state[LED_STATE_NORMAL].g = g;  g_led_state[LED_STATE_NORMAL].b = b;
    g_led_state[LED_STATE_ALARM].r  = r;  g_led_state[LED_STATE_ALARM].g  = g;  g_led_state[LED_STATE_ALARM].b  = b;
    status_led_update();
    settings_save_u32(NVS_KEY_LED_R, r);
    settings_save_u32(NVS_KEY_LED_G, g);
    settings_save_u32(NVS_KEY_LED_B, b);
    settings_save_u32(NVS_KEY_LED_N_R, r); settings_save_u32(NVS_KEY_LED_N_G, g); settings_save_u32(NVS_KEY_LED_N_B, b);
    settings_save_u32(NVS_KEY_LED_A_R, r); settings_save_u32(NVS_KEY_LED_A_G, g); settings_save_u32(NVS_KEY_LED_A_B, b);
    settings_commit();

}

void app_controller_set_led_brightness(uint8_t pct)
{
    if (pct < 1) pct = 1;
    if (pct > 100) pct = 100;
    g_led_brightness = pct;
    status_led_update();
    settings_save_u32(NVS_KEY_LED_BRIGHT, pct);
    settings_commit();

}

void app_controller_get_led_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (r) *r = g_led_r;
    if (g) *g = g_led_g;
    if (b) *b = g_led_b;
}

uint8_t app_controller_get_led_brightness(void)
{
    return g_led_brightness;
}

void app_controller_set_led_effect(uint8_t effect)
{
    if (effect > LED_EFFECT_RAINBOW) return;
    g_led_state[LED_STATE_NORMAL].effect = effect;
    settings_save_u8(NVS_KEY_LED_N_EFF, effect);
    settings_commit();
    status_led_update();
}

void app_controller_set_led_state_cfg(led_state_id_t id, const led_state_cfg_t *cfg)
{
    if (id >= LED_STATE_COUNT || !cfg) return;
    ESP_LOGI(TAG, "LED save: id=%d R=%d G=%d B=%d E=%d", id, cfg->r, cfg->g, cfg->b, cfg->effect);
    g_led_state[id] = *cfg;
    if (id == LED_STATE_NORMAL) {
        g_led_r = cfg->r; g_led_g = cfg->g; g_led_b = cfg->b;
    }

    static const char *state_keys[LED_STATE_COUNT][4] = {
        {NVS_KEY_LED_N_R, NVS_KEY_LED_N_G, NVS_KEY_LED_N_B, NVS_KEY_LED_N_EFF},
        {NVS_KEY_LED_A_R, NVS_KEY_LED_A_G, NVS_KEY_LED_A_B, NVS_KEY_LED_A_EFF},
        {NVS_KEY_LED_S_R, NVS_KEY_LED_S_G, NVS_KEY_LED_S_B, NVS_KEY_LED_S_EFF},
        {NVS_KEY_LED_F_R, NVS_KEY_LED_F_G, NVS_KEY_LED_F_B, NVS_KEY_LED_F_EFF},
        {NVS_KEY_LED_C_R, NVS_KEY_LED_C_G, NVS_KEY_LED_C_B, NVS_KEY_LED_C_EFF},
        {NVS_KEY_LED_O_R, NVS_KEY_LED_O_G, NVS_KEY_LED_O_B, NVS_KEY_LED_O_EFF},
    };
    settings_save_u8(state_keys[id][0], cfg->r);
    settings_save_u8(state_keys[id][1], cfg->g);
    settings_save_u8(state_keys[id][2], cfg->b);
    settings_save_u8(state_keys[id][3], cfg->effect);
    settings_commit();
    status_led_update();
    /* Sync to HA: publish the changed state's config */
}

const led_state_cfg_t *app_controller_get_led_state_cfg(led_state_id_t id)
{
    if (id >= LED_STATE_COUNT) return &g_led_state[0];
    return &g_led_state[id];
}

/* ── HA per-state LED config (mirrors web "状态指示灯" card) ──────── */

void app_controller_set_led_cfg_state(uint8_t state_id)
{
    if (state_id >= LED_STATE_COUNT) return;
    g_led_cfg_state_id = state_id;
}

uint8_t app_controller_get_led_cfg_state(void) { return g_led_cfg_state_id; }

void app_controller_set_led_state_rgb(uint8_t state_id, uint8_t r, uint8_t g, uint8_t b)
{
    if (state_id >= LED_STATE_COUNT) return;
    g_led_state[state_id].r = r;
    g_led_state[state_id].g = g;
    g_led_state[state_id].b = b;
    if (state_id == LED_STATE_NORMAL) { g_led_r = r; g_led_g = g; g_led_b = b; }
    static const char *keys[LED_STATE_COUNT][3] = {
        {NVS_KEY_LED_N_R, NVS_KEY_LED_N_G, NVS_KEY_LED_N_B},
        {NVS_KEY_LED_A_R, NVS_KEY_LED_A_G, NVS_KEY_LED_A_B},
        {NVS_KEY_LED_S_R, NVS_KEY_LED_S_G, NVS_KEY_LED_S_B},
        {NVS_KEY_LED_F_R, NVS_KEY_LED_F_G, NVS_KEY_LED_F_B},
        {NVS_KEY_LED_C_R, NVS_KEY_LED_C_G, NVS_KEY_LED_C_B},
        {NVS_KEY_LED_O_R, NVS_KEY_LED_O_G, NVS_KEY_LED_O_B},
    };
    settings_save_u8(keys[state_id][0], r);
    settings_save_u8(keys[state_id][1], g);
    settings_save_u8(keys[state_id][2], b);
    settings_commit();
    if (state_id == g_led_cfg_state_id) {
    }
    status_led_update();
}

void app_controller_set_led_state_effect(uint8_t state_id, uint8_t eff)
{
    if (state_id >= LED_STATE_COUNT || eff > 4) return;
    g_led_state[state_id].effect = eff;
    static const char *keys[LED_STATE_COUNT] = {
        NVS_KEY_LED_N_EFF, NVS_KEY_LED_A_EFF, NVS_KEY_LED_S_EFF,
        NVS_KEY_LED_F_EFF, NVS_KEY_LED_C_EFF, NVS_KEY_LED_O_EFF,
    };
    settings_save_u8(keys[state_id], eff);
    settings_commit();
    if (state_id == g_led_cfg_state_id) {
    }
    status_led_update();
}

void app_controller_set_ota_in_progress(bool in_progress) {
    g_ota_in_progress = in_progress;
    status_led_update();
}

bool app_controller_get_status_led(void)
{
    return g_led_on;
}

bool app_controller_is_shutdown(void)
{
    return g_shutdown;
}

bool app_controller_is_wifi_ready(void)
{
    return g_wifi_ready;
}

/* ── Deep sleep (full power off) ─────────────────────────────────── */
void app_controller_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep...");

    if (g_power_on) {
        motor_stop();
        g_power_on = false;
    }

    if (g_lcd_panel) {
        esp_lcd_panel_disp_on_off(g_lcd_panel, false);
    }

    speaker_power_off();

    esp_sleep_enable_timer_wakeup(60 * 1000000ULL);
    esp_deep_sleep_start();
}

float app_controller_get_indoor_temp(void)
{
    return g_indoor_temp;
}
