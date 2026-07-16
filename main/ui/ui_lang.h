#pragma once

#include <stdint.h>

typedef enum {
    LANG_CN = 0,
    LANG_EN,
} ui_lang_t;

typedef enum {
    /* ── Dashboard screen ────────────────────────────── */
    TXT_DASH_TITLE,
    TXT_TEMP,
    TXT_WAITING,
    TXT_FAN_FMT,
    TXT_WIFI_FMT,
    TXT_MQTT_FMT,
    TXT_SSID_FMT,
    TXT_OFFLINE,

    /* ── Fan screen ──────────────────────────────────── */
    TXT_FAN_SPEED,
    TXT_OFF,
    TXT_GEAR_1,
    TXT_GEAR_2,
    TXT_GEAR_3,
    TXT_GEAR_4,
    TXT_GEAR_5,
    TXT_SPEED_LABEL,
    TXT_POWER_ON,
    TXT_POWER_OFF,
    TXT_RPM_FMT,

    /* ── Brightness screen ───────────────────────────── */
    TXT_BRIGHTNESS,
    TXT_DIM,
    TXT_BRIGHT,
    TXT_BACKLIGHT_NOTE,

    /* ── Network screen ──────────────────────────────── */
    TXT_NETWORK,
    TXT_THEME_FMT,
    TXT_THEME_DARK,
    TXT_THEME_LIGHT,
    TXT_THEME_AUTO,
    TXT_STATUS,
    TXT_QR_CODE,
    TXT_MANUAL_CONFIG,
    TXT_WIFI_NAME,
    TXT_WIFI_PASS,
    TXT_CONNECT,
    TXT_WIFI_SETUP,
    TXT_MQTT_CONNECTED,
    TXT_MQTT_OFFLINE,
    TXT_MQTT_OK,
    TXT_MQTT_FAIL,

    /* ── MQTT config ─────────────────────────────────── */
    TXT_MQTT_CONFIG,
    TXT_MQTT_URI,
    TXT_MQTT_USER,
    TXT_MQTT_PASS,
    TXT_MQTT_TEST,
    TXT_MQTT_SAVE,
    TXT_MQTT_TESTING,
    TXT_MQTT_TEST_OK,
    TXT_MQTT_TEST_FAIL,
    TXT_MQTT_SAVED,
    TXT_PROVISION,
    TXT_RESET_WIFI,
    TXT_SCAN_QR,
    TXT_SCANNING,
    TXT_PROV_HINT,
    TXT_WIFI_CONNECTING,
    TXT_WIFI_CONNECTED,
    TXT_WIFI_CONNECT_FAILED,

    /* ── Power screen ────────────────────────────────── */
    TXT_POWER,
    TXT_SCREEN_OFF,
    TXT_DOUBLE_TAP,
    TXT_SHUTDOWN,
    TXT_SHUTDOWN_DESC,
    TXT_MONITORING_STOP,
    TXT_SHUTTING_DOWN,

    /* ── Power schedule ──────────────────────────────── */
    TXT_SCHEDULE_TITLE,
    TXT_SCHED_ENABLE,
    TXT_SCHED_OFF_TIME,
    TXT_SCHED_ON_TIME,
    TXT_SCHED_WILL,
    TXT_SCHED_DISABLED,
    TXT_SCHED_SAVED_S,
    TXT_TODAY,
    TXT_TOMORROW,
    TXT_DAY_AFTER,
    TXT_EVERY_DAY,
    TXT_WEEKDAYS,
    TXT_WEEKENDS,
    TXT_MON,
    TXT_TUE,
    TXT_WED,
    TXT_THU,
    TXT_FRI,
    TXT_SAT,
    TXT_SUN,
    TXT_CUSTOM,          /* 自定义 */

    /* ── Provision prompt ────────────────────────────── */
    TXT_WELCOME,
    TXT_PROV_MSG,
    TXT_CONNECT_WIFI,
    TXT_SKIP,

    /* ── Common ──────────────────────────────────────── */
    TXT_OK,
    TXT_COLLAPSE,
    TXT_UNCONFIGURED,
    TXT_NOT_CONNECTED,
    TXT_TAP_TO_CLOSE,

    /* ── AQI / TVOC labels ───────────────────────────── */
    TXT_AQI_EXCELLENT,
    TXT_AQI_GOOD,
    TXT_AQI_MODERATE,
    TXT_AQI_POOR,
    TXT_AQI_HAZARDOUS,

    /* ── IP format ───────────────────────────────────── */
    TXT_IP_FMT,

    /* ── Settings page ────────────────────────────────── */
    TXT_LANGUAGE,
    TXT_THEME,
    TXT_RESET,
    TXT_RESET_HINT,
    TXT_TIME,
    TXT_SUNRISE,
    TXT_SUNSET,

    /* ── Sound settings page ──────────────────────────── */
    TXT_SOUND_SETTINGS,
    TXT_KEY_SOUND,
    TXT_POWER_SOUND_LABEL,
    TXT_KEY_VOLUME,
    TXT_POWER_VOLUME_LABEL,
    TXT_ALARM_SOUND,
    TXT_VOLUME,

    /* ── IP connectivity test ─────────────────────────── */
    TXT_IP_TESTING,
    TXT_IP_CONNECTED,
    TXT_IP_NO_INTERNET,

    /* ── Day selector ─────────────────────────────────── */
    TXT_SELECT_DAY,

    TXT_COUNT
} ui_text_key_t;

void ui_lang_init(void);
void ui_lang_set(ui_lang_t lang);
ui_lang_t ui_lang_get(void);
const char *ui_lang_str(ui_text_key_t key);
