#include "settings.h"
#include "board.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "settings";
static nvs_handle_t g_nvs = 0;

void settings_init(void)
{
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &g_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
}

void settings_factory_defaults(app_settings_t *out)
{
    memset(out, 0, sizeof(*out));
    out->motor_speed_min = 20;
    out->motor_speed_max = 100;
    out->tvoc_alarm_ugm3 = 500;
    out->co2_alarm_ppm = 1000;
    out->ch2o_alarm_ugm3 = 100;
    out->auto_fan_enable = true;
    out->brightness = 80;
    out->child_lock = false;
    out->key_volume = 70;
    out->key_sound_enable = true;
    out->power_volume = 70;
    out->power_sound_enable = true;
    out->alarm_volume = 70;
    out->alarm_sound_enable = true;
    out->alarm_cooldown_s = 60;
    out->home_screen = 0;  /* UI_SCREEN_HOME */
    out->key_melody = 0;
    out->power_on_melody = 0;
    out->power_off_melody = 0;
    out->alarm_melody = 0;
    snprintf(out->device_name, sizeof(out->device_name), "AiRFLOW");
    snprintf(out->device_id, sizeof(out->device_id), "esp32_ap_01");
}

void settings_load(app_settings_t *out)
{
    settings_factory_defaults(out);

    size_t len;
    len = sizeof(out->wifi_ssid);
    nvs_get_str(g_nvs, NVS_KEY_WIFI_SSID, out->wifi_ssid, &len);
    len = sizeof(out->wifi_pass);
    nvs_get_str(g_nvs, NVS_KEY_WIFI_PASS, out->wifi_pass, &len);
    len = sizeof(out->mqtt_uri);
    nvs_get_str(g_nvs, NVS_KEY_MQTT_URI, out->mqtt_uri, &len);
    len = sizeof(out->mqtt_user);
    nvs_get_str(g_nvs, NVS_KEY_MQTT_USER, out->mqtt_user, &len);
    len = sizeof(out->mqtt_pass);
    nvs_get_str(g_nvs, NVS_KEY_MQTT_PASS, out->mqtt_pass, &len);
    len = sizeof(out->device_name);
    nvs_get_str(g_nvs, NVS_KEY_DEVICE_NAME, out->device_name, &len);
    len = sizeof(out->device_id);
    nvs_get_str(g_nvs, NVS_KEY_DEVICE_ID, out->device_id, &len);

    uint32_t u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_MOTOR_MIN, &u32) == ESP_OK) out->motor_speed_min = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_MOTOR_MAX, &u32) == ESP_OK) out->motor_speed_max = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_TVOC_ALARM, &u32) == ESP_OK) out->tvoc_alarm_ugm3 = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_CO2_ALARM, &u32) == ESP_OK) out->co2_alarm_ppm = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_CH2O_ALARM, &u32) == ESP_OK) out->ch2o_alarm_ugm3 = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_AUTO_FAN_ENABLE, &u32) == ESP_OK) out->auto_fan_enable = (u32 != 0);
    if (nvs_get_u32(g_nvs, NVS_KEY_BRIGHTNESS, &u32) == ESP_OK) out->brightness = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_CHILD_LOCK, &u32) == ESP_OK) out->child_lock = (u32 != 0);
    if (nvs_get_u32(g_nvs, NVS_KEY_KEY_VOLUME, &u32) == ESP_OK) out->key_volume = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_KEY_SOUND_EN, &u32) == ESP_OK) out->key_sound_enable = (u32 != 0);
    if (nvs_get_u32(g_nvs, NVS_KEY_POWER_VOLUME, &u32) == ESP_OK) out->power_volume = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_POWER_SOUND_EN, &u32) == ESP_OK) out->power_sound_enable = (u32 != 0);
    if (nvs_get_u32(g_nvs, NVS_KEY_ALARM_VOLUME, &u32) == ESP_OK) out->alarm_volume = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_ALARM_SOUND_EN, &u32) == ESP_OK) out->alarm_sound_enable = (u32 != 0);
    if (nvs_get_u32(g_nvs, NVS_KEY_ALARM_COOLDOWN, &u32) == ESP_OK) out->alarm_cooldown_s = (uint16_t)u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_HOME_SCREEN, &u32) == ESP_OK) out->home_screen = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_KEY_MELODY, &u32) == ESP_OK) out->key_melody = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_PWRON_MELODY, &u32) == ESP_OK) out->power_on_melody = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_PWROFF_MELODY, &u32) == ESP_OK) out->power_off_melody = u32;
    if (nvs_get_u32(g_nvs, NVS_KEY_ALARM_MELODY, &u32) == ESP_OK) out->alarm_melody = u32;

    ESP_LOGI(TAG, "Settings loaded");
}

void settings_save(const app_settings_t *s)
{
    settings_save_str(NVS_KEY_WIFI_SSID, s->wifi_ssid);
    settings_save_str(NVS_KEY_WIFI_PASS, s->wifi_pass);
    settings_save_str(NVS_KEY_MQTT_URI, s->mqtt_uri);
    settings_save_str(NVS_KEY_MQTT_USER, s->mqtt_user);
    settings_save_str(NVS_KEY_MQTT_PASS, s->mqtt_pass);
    settings_save_str(NVS_KEY_DEVICE_NAME, s->device_name);
    settings_save_str(NVS_KEY_DEVICE_ID, s->device_id);
    settings_save_u32(NVS_KEY_MOTOR_MIN, s->motor_speed_min);
    settings_save_u32(NVS_KEY_MOTOR_MAX, s->motor_speed_max);
    settings_save_u32(NVS_KEY_TVOC_ALARM, s->tvoc_alarm_ugm3);
    settings_save_u32(NVS_KEY_CO2_ALARM, s->co2_alarm_ppm);
    settings_save_u32(NVS_KEY_CH2O_ALARM, s->ch2o_alarm_ugm3);
    settings_save_u32(NVS_KEY_AUTO_FAN_ENABLE, s->auto_fan_enable ? 1 : 0);
    settings_save_u32(NVS_KEY_BRIGHTNESS, s->brightness);
    settings_save_u32(NVS_KEY_CHILD_LOCK, s->child_lock ? 1 : 0);
    settings_save_u32(NVS_KEY_KEY_VOLUME, s->key_volume);
    settings_save_u32(NVS_KEY_KEY_SOUND_EN, s->key_sound_enable ? 1 : 0);
    settings_save_u32(NVS_KEY_POWER_VOLUME, s->power_volume);
    settings_save_u32(NVS_KEY_POWER_SOUND_EN, s->power_sound_enable ? 1 : 0);
    settings_save_u32(NVS_KEY_ALARM_VOLUME, s->alarm_volume);
    settings_save_u32(NVS_KEY_ALARM_SOUND_EN, s->alarm_sound_enable ? 1 : 0);
    settings_save_u32(NVS_KEY_ALARM_COOLDOWN, s->alarm_cooldown_s);
    settings_save_u32(NVS_KEY_HOME_SCREEN, s->home_screen);
    settings_save_u32(NVS_KEY_KEY_MELODY, s->key_melody);
    settings_save_u32(NVS_KEY_PWRON_MELODY, s->power_on_melody);
    settings_save_u32(NVS_KEY_PWROFF_MELODY, s->power_off_melody);
    settings_save_u32(NVS_KEY_ALARM_MELODY, s->alarm_melody);

    nvs_commit(g_nvs);
    ESP_LOGI(TAG, "Settings saved");
}

void settings_save_str(const char *key, const char *value)
{
    if (!g_nvs || !key) return;
    if (value && value[0]) {
        nvs_set_str(g_nvs, key, value);
    } else {
        nvs_erase_key(g_nvs, key);
    }
}

void settings_save_u8(const char *key, uint8_t value)
{
    if (g_nvs) nvs_set_u8(g_nvs, key, value);
}

void settings_save_u32(const char *key, uint32_t value)
{
    if (g_nvs) nvs_set_u32(g_nvs, key, value);
}

esp_err_t settings_get_str(const char *key, char *out, size_t max_len)
{
    if (!g_nvs) return ESP_FAIL;
    return nvs_get_str(g_nvs, key, out, &max_len);
}

esp_err_t settings_get_u8(const char *key, uint8_t *out)
{
    if (!g_nvs) return ESP_FAIL;
    return nvs_get_u8(g_nvs, key, out);
}

esp_err_t settings_get_u32(const char *key, uint32_t *out)
{
    if (!g_nvs) return ESP_FAIL;
    return nvs_get_u32(g_nvs, key, out);
}

void settings_erase_key(const char *key)
{
    if (g_nvs && key) nvs_erase_key(g_nvs, key);
}

void settings_commit(void)
{
    if (g_nvs) nvs_commit(g_nvs);
}

void settings_erase_all(void)
{
    if (g_nvs) {
        nvs_erase_all(g_nvs);
        nvs_commit(g_nvs);
    }
}
