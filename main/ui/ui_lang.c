#include "ui_lang.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stddef.h>

static const char *TAG = "lang";

/* ── Forward declarations — each screen calls its _lang_update() below ── */
void ui_screen_home_lang_update(void);
void ui_screen_settings_lang_update(void);
void ui_screen_network_lang_update(void);
void ui_screen_power_lang_update(void);
void ui_screen_sound_lang_update(void);
void ui_provision_prompt_lang_update(void);
void ui_theme_lang_update(void);

static ui_lang_t g_lang = LANG_CN;

/* ── Chinese strings ────────────────────────────────────────────────── */
static const char *s_cn[TXT_COUNT] = {
    [TXT_DASH_TITLE]      = "空气质量",
    [TXT_TEMP]            = "温度",
    [TXT_WAITING]         = "等待中...",
    [TXT_FAN_FMT]         = "风扇 %u%%",
    [TXT_WIFI_FMT]        = "WiFi %s",
    [TXT_MQTT_FMT]        = "MQTT %s",
    [TXT_SSID_FMT]        = "SSID: %s",
    [TXT_OFFLINE]         = "离线",

    [TXT_FAN_SPEED]       = "风扇速度",
    [TXT_OFF]             = "关闭",
    [TXT_GEAR_1]          = "1档",
    [TXT_GEAR_2]          = "2档",
    [TXT_GEAR_3]          = "3档",
    [TXT_GEAR_4]          = "4档",
    [TXT_GEAR_5]          = "5档",
    [TXT_SPEED_LABEL]     = "速度",
    [TXT_POWER_ON]        = "开机",
    [TXT_POWER_OFF]       = "关机",
    [TXT_RPM_FMT]         = "%u 转/分",

    [TXT_BRIGHTNESS]      = "亮度",
    [TXT_DIM]             = "暗",
    [TXT_BRIGHT]          = "亮",
    [TXT_BACKLIGHT_NOTE]  = "背光硬件未连接",

    [TXT_NETWORK]         = "网络",
    [TXT_THEME_FMT]       = "主题: %s",
    [TXT_THEME_DARK]      = "深色",
    [TXT_THEME_LIGHT]     = "浅色",
    [TXT_THEME_AUTO]      = "自动",
    [TXT_STATUS]          = "状态",
    [TXT_QR_CODE]         = "QR\n码",
    [TXT_MANUAL_CONFIG]   = "手动配置",
    [TXT_WIFI_NAME]       = "WiFi名称",
    [TXT_WIFI_PASS]       = "WiFi密码",
    [TXT_CONNECT]         = "连接",
    [TXT_WIFI_SETUP]      = "WiFi设置",
    [TXT_MQTT_CONNECTED]  = "MQTT: 已连接",
    [TXT_MQTT_OFFLINE]    = "MQTT: 未连接",
    [TXT_MQTT_OK]         = "MQTT OK",
    [TXT_MQTT_FAIL]       = "MQTT ERR",

    [TXT_MQTT_CONFIG]     = "MQTT 配置",
    [TXT_MQTT_URI]        = "代理地址",
    [TXT_MQTT_USER]       = "用户名 (可选)",
    [TXT_MQTT_PASS]       = "密码 (可选)",
    [TXT_MQTT_TEST]       = "测试连接",
    [TXT_MQTT_SAVE]       = "保存",
    [TXT_MQTT_TESTING]    = "测试中...",
    [TXT_MQTT_TEST_OK]    = "连接成功",
    [TXT_MQTT_TEST_FAIL]  = "连接失败",
    [TXT_MQTT_SAVED]      = "已保存",
    [TXT_PROVISION]       = "配网",
    [TXT_PROV_HINT]       = "首次使用通过手机App配网",
    [TXT_RESET_WIFI]      = "重置WiFi",
    [TXT_WIFI_CONNECTING]      = "连接中...",
    [TXT_WIFI_CONNECTED]       = "连接成功",
    [TXT_WIFI_CONNECT_FAILED]  = "连接失败",
    [TXT_SCAN_QR]         = "扫码连接",
    [TXT_SCANNING]        = "扫描中...",

    [TXT_POWER]           = "电源",
    [TXT_SCREEN_OFF]      = "关闭屏幕",
    [TXT_DOUBLE_TAP]      = "双击唤醒",
    [TXT_SHUTDOWN]        = "关机",
    [TXT_SHUTDOWN_DESC]   = "停止传感器和风扇，双击唤醒",
    [TXT_MONITORING_STOP] = "所有空气监测将停止",
    [TXT_SHUTTING_DOWN]   = "关机中",

    [TXT_SCHEDULE_TITLE]  = "定时开关机",
    [TXT_SCHED_ENABLE]    = "启用",
    [TXT_SCHED_OFF_TIME]  = "关机",
    [TXT_SCHED_ON_TIME]   = "开机",
    [TXT_SCHED_WILL]      = "将在%s %02d:%02d 关机，%s %02d:%02d 开机",
    [TXT_SCHED_DISABLED]  = "定时未启用",
    [TXT_SCHED_SAVED_S]   = "已保存",
    [TXT_TODAY]          = "今天",
    [TXT_TOMORROW]       = "明天",
    [TXT_DAY_AFTER]      = "后天",
    [TXT_EVERY_DAY]      = "每天",
    [TXT_WEEKDAYS]       = "工作日",
    [TXT_WEEKENDS]       = "周末",
    [TXT_MON]            = "周一",
    [TXT_TUE]            = "周二",
    [TXT_WED]            = "周三",
    [TXT_THU]            = "周四",
    [TXT_FRI]            = "周五",
    [TXT_SAT]            = "周六",
    [TXT_SUN]            = "周日",
    [TXT_CUSTOM]          = "自定义",

    [TXT_WELCOME]         = "WiFi 设置",
    [TXT_PROV_MSG]        = "连接到热点\n完成WiFi设置",
    [TXT_CONNECT_WIFI]    = "连接WiFi",
    [TXT_SKIP]            = "跳过",

    [TXT_OK]              = "确定",
    [TXT_COLLAPSE]        = "关闭",
    [TXT_UNCONFIGURED]    = "未配置",
    [TXT_NOT_CONNECTED]   = "未连接",
    [TXT_TAP_TO_CLOSE]    = "点击空白关闭",

    [TXT_AQI_EXCELLENT]   = "优秀",
    [TXT_AQI_GOOD]        = "良好",
    [TXT_AQI_MODERATE]    = "中等",
    [TXT_AQI_POOR]        = "较差",
    [TXT_AQI_HAZARDOUS]   = "危险",

    [TXT_IP_FMT]         = "IP: %s",

    [TXT_LANGUAGE]       = "语言",
    [TXT_THEME]          = "主题",
    [TXT_RESET]          = "点三次恢复出厂",
    [TXT_RESET_HINT]     = "三击重置所有设定",
    [TXT_TIME]           = "时间",
    [TXT_SUNRISE]        = "日出",
    [TXT_SUNSET]         = "日落",

    [TXT_SOUND_SETTINGS]      = "声音",
    [TXT_KEY_SOUND]           = "按键声音",
    [TXT_POWER_SOUND_LABEL]   = "开关机声音",
    [TXT_KEY_VOLUME]          = "按键音量",
    [TXT_POWER_VOLUME_LABEL]  = "开关机音量",
    [TXT_ALARM_SOUND]         = "通知",
    [TXT_VOLUME]              = "音量",

    [TXT_IP_TESTING]      = "测试中...",
    [TXT_IP_CONNECTED]    = "已连通",
    [TXT_IP_NO_INTERNET]  = "已离线",

    [TXT_SELECT_DAY]      = "选择日期",
};

/* ── English strings ─────────────────────────────────────────────────── */
static const char *s_en[TXT_COUNT] = {
    [TXT_DASH_TITLE]      = "Air Quality",
    [TXT_TEMP]            = "Temp",
    [TXT_WAITING]         = "Waiting...",
    [TXT_FAN_FMT]         = "Fan %u%%",
    [TXT_WIFI_FMT]        = "WiFi %s",
    [TXT_MQTT_FMT]        = "MQTT %s",
    [TXT_SSID_FMT]        = "SSID: %s",
    [TXT_OFFLINE]         = "Offline",

    [TXT_FAN_SPEED]       = "Fan Speed",
    [TXT_OFF]             = "Off",
    [TXT_GEAR_1]          = "Gear 1",
    [TXT_GEAR_2]          = "Gear 2",
    [TXT_GEAR_3]          = "Gear 3",
    [TXT_GEAR_4]          = "Gear 4",
    [TXT_GEAR_5]          = "Gear 5",
    [TXT_SPEED_LABEL]     = "Speed",
    [TXT_POWER_ON]        = "Power On",
    [TXT_POWER_OFF]       = "Power Off",
    [TXT_RPM_FMT]         = "%u RPM",

    [TXT_BRIGHTNESS]      = "Brightness",
    [TXT_DIM]             = "Dim",
    [TXT_BRIGHT]          = "Bright",
    [TXT_BACKLIGHT_NOTE]  = "Backlight HW not connected",

    [TXT_NETWORK]         = "Network",
    [TXT_THEME_FMT]       = "Theme: %s",
    [TXT_THEME_DARK]      = "Dark",
    [TXT_THEME_LIGHT]     = "Light",
    [TXT_THEME_AUTO]      = "Auto",
    [TXT_STATUS]          = "Status",
    [TXT_QR_CODE]         = "QR\nCode",
    [TXT_MANUAL_CONFIG]   = "Manual Config",
    [TXT_WIFI_NAME]       = "WiFi Name",
    [TXT_WIFI_PASS]       = "WiFi Password",
    [TXT_CONNECT]         = "Connect",
    [TXT_WIFI_SETUP]      = "WiFi Setup",
    [TXT_MQTT_CONNECTED]  = "MQTT: Connected",
    [TXT_MQTT_OFFLINE]    = "MQTT: Offline",
    [TXT_MQTT_OK]         = "MQTT OK",
    [TXT_MQTT_FAIL]       = "MQTT ERR",

    [TXT_MQTT_CONFIG]     = "MQTT Config",
    [TXT_MQTT_URI]        = "Broker URI",
    [TXT_MQTT_USER]       = "Username (optional)",
    [TXT_MQTT_PASS]       = "Password (optional)",
    [TXT_MQTT_TEST]       = "Test",
    [TXT_MQTT_SAVE]       = "Save",
    [TXT_MQTT_TESTING]    = "Testing...",
    [TXT_MQTT_TEST_OK]    = "Connected",
    [TXT_MQTT_TEST_FAIL]  = "Failed",
    [TXT_MQTT_SAVED]      = "Saved",
    [TXT_PROVISION]       = "Provision",
    [TXT_PROV_HINT]       = "Setup via mobile app (first use)",
    [TXT_RESET_WIFI]      = "Reset WiFi",
    [TXT_WIFI_CONNECTING]      = "Connecting...",
    [TXT_WIFI_CONNECTED]       = "Connected",
    [TXT_WIFI_CONNECT_FAILED]  = "Connection Failed",
    [TXT_SCAN_QR]         = "Scan QR",
    [TXT_SCANNING]        = "Scanning...",

    [TXT_POWER]           = "Power",
    [TXT_SCREEN_OFF]      = "Screen Off",
    [TXT_DOUBLE_TAP]      = "Double-tap to wake",
    [TXT_SHUTDOWN]        = "Shutdown",
    [TXT_SHUTDOWN_DESC]   = "Stop sensors & fan, double-tap to wake",
    [TXT_MONITORING_STOP] = "All air monitoring will stop",
    [TXT_SHUTTING_DOWN]   = "Shutting down...",

    [TXT_SCHEDULE_TITLE]  = "Scheduled Power",
    [TXT_SCHED_ENABLE]    = "Enable",
    [TXT_SCHED_OFF_TIME]  = "Off",
    [TXT_SCHED_ON_TIME]   = "On",
    [TXT_SCHED_WILL]      = "Off %s%02d:%02d, On %s%02d:%02d",
    [TXT_SCHED_DISABLED]  = "Schedule disabled",
    [TXT_SCHED_SAVED_S]   = "Saved",
    [TXT_TODAY]          = "Today",
    [TXT_TOMORROW]       = "Tomorrow",
    [TXT_DAY_AFTER]      = "Day after",
    [TXT_EVERY_DAY]      = "Every Day",
    [TXT_WEEKDAYS]       = "Weekdays",
    [TXT_WEEKENDS]       = "Weekends",
    [TXT_MON]            = "Mon",
    [TXT_TUE]            = "Tue",
    [TXT_WED]            = "Wed",
    [TXT_THU]            = "Thu",
    [TXT_FRI]            = "Fri",
    [TXT_SAT]            = "Sat",
    [TXT_SUN]            = "Sun",
    [TXT_CUSTOM]          = "Custom",

    [TXT_WELCOME]         = "WiFi Setup",
    [TXT_PROV_MSG]        = "Connect to device hotspot\nto complete WiFi setup",
    [TXT_CONNECT_WIFI]    = "Connect WiFi",
    [TXT_SKIP]            = "Skip",

    [TXT_OK]              = "OK",
    [TXT_COLLAPSE]        = "Close",
    [TXT_UNCONFIGURED]    = "Not configured",
    [TXT_NOT_CONNECTED]   = "Not connected",
    [TXT_TAP_TO_CLOSE]    = "Tap blank to close",

    [TXT_AQI_EXCELLENT]   = "Excellent",
    [TXT_AQI_GOOD]        = "Good",
    [TXT_AQI_MODERATE]    = "Moderate",
    [TXT_AQI_POOR]        = "Poor",
    [TXT_AQI_HAZARDOUS]   = "Hazardous",

    [TXT_IP_FMT]         = "IP: %s",

    [TXT_LANGUAGE]       = "Language",
    [TXT_THEME]          = "Theme",
    [TXT_RESET]          = "Tap 3× to Reset",
    [TXT_RESET_HINT]     = "Triple-tap to reset",
    [TXT_TIME]           = "Time",
    [TXT_SUNRISE]        = "Sunrise",
    [TXT_SUNSET]         = "Sunset",

    [TXT_SOUND_SETTINGS]      = "Sound",
    [TXT_KEY_SOUND]           = "Key Sound",
    [TXT_POWER_SOUND_LABEL]   = "Power Sound",
    [TXT_KEY_VOLUME]          = "Key Volume",
    [TXT_POWER_VOLUME_LABEL]  = "Power Volume",
    [TXT_ALARM_SOUND]         = "Alarm",
    [TXT_VOLUME]              = "Volume",

    [TXT_IP_TESTING]      = "Testing...",
    [TXT_IP_CONNECTED]    = "Online",
    [TXT_IP_NO_INTERNET]  = "Offline",

    [TXT_SELECT_DAY]      = "Select Day",
};

/* ── NVS key ─────────────────────────────────────────────────────────── */
#define NVS_NAMESPACE "ui"
#define NVS_KEY_LANG  "lang2"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void lang_update_all(void)
{
    ui_screen_home_lang_update();
    ui_screen_settings_lang_update();
    ui_screen_network_lang_update();
    ui_screen_power_lang_update();
    ui_screen_sound_lang_update();
    ui_provision_prompt_lang_update();
    ui_theme_lang_update();
}

/* ── API ──────────────────────────────────────────────────────────────── */

void ui_lang_init(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t val = LANG_CN;
        if (nvs_get_u8(handle, NVS_KEY_LANG, &val) == ESP_OK) {
            g_lang = (val <= LANG_EN) ? (ui_lang_t)val : LANG_CN;
        }
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "language: %s", g_lang == LANG_CN ? "CN" : "EN");
}

void ui_lang_set(ui_lang_t lang)
{
    if (lang == g_lang) return;
    g_lang = lang;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_LANG, (uint8_t)lang);
        nvs_commit(handle);
        nvs_close(handle);
    }

    lang_update_all();
    ESP_LOGI(TAG, "switched to %s", lang == LANG_CN ? "CN" : "EN");
}

ui_lang_t ui_lang_get(void) { return g_lang; }

const char *ui_lang_str(ui_text_key_t key)
{
    if (key >= TXT_COUNT) return "";
    const char *s = (g_lang == LANG_CN) ? s_cn[key] : s_en[key];
    return s ? s : "";
}
