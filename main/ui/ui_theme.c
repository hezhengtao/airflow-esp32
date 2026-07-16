#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_apple_anim.h"
#include "ui_manager.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>
#include <time.h>

#define TAG "theme"

/* ── Theme state ────────────────────────────────────────────────── */
static ui_theme_mode_t g_theme_mode = UI_THEME_LIGHT;

#define NVS_NAMESPACE "ui"
#define NVS_KEY_THEME "theme"
#define DEFAULT_LAT   39.9   /* Beijing */
#define DEFAULT_LON   116.4
static lv_timer_t *g_auto_timer = NULL;   /* one-shot: fires at next sunrise/sunset */
static lv_timer_t *g_poll_timer = NULL;   /* 5s poll while waiting for SNTP */
static volatile bool s_time_synced = false; /* set by SNTP notification callback */

/* ── SNTP init — called lazily when network is ready ─────────────── */
static bool sntp_done = false;

static void sntp_sync_cb(struct timeval *tv)
{
    s_time_synced = true;
    struct tm ti;
    time_t now = tv->tv_sec;
    localtime_r(&now, &ti);
    ESP_LOGI(TAG, "SNTP synced: %04d-%02d-%02d %02d:%02d:%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
}

static void sntp_init_once(void)
{
    if (sntp_done) {
        time_t now = time(NULL);
        struct tm ti;
        localtime_r(&now, &ti);
        if (ti.tm_year + 1900 >= 2025) return;  /* already synced */
        /* Not synced yet, but SNTP is already running — do NOT re-init.
         * Re-init asserts "mode must not be set while client is running". */
        ESP_LOGW(TAG, "SNTP still syncing (year=%d), waiting...", ti.tm_year + 1900);
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_interval(600000);  /* re-sync every 10 min (default 1h too long, RTC drifts) */
    /* Use IP addresses first to avoid DNS dependency */
    esp_sntp_setservername(0, "203.107.6.88");   /* ntp.aliyun.com */
    esp_sntp_setservername(1, "120.25.115.20");  /* ntp1.aliyun.com */
    esp_sntp_setservername(2, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();
    sntp_done = true;

    /* Log current time state */
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    ESP_LOGI(TAG, "SNTP initialised, current time: %04d-%02d-%02d %02d:%02d:%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
}

/* ── Sunset calculation (simplified, no atmospheric correction) ──── */

static double to_rad(double deg) { return deg * M_PI / 180.0; }
static double to_deg(double rad) { return rad * 180.0 / M_PI; }

static void calc_sun_times(int *sunrise_min, int *sunset_min)
{
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    /* If NTP hasn't synced yet (year < 2025), use defaults */
    if (ti.tm_year + 1900 < 2025) {
        *sunrise_min  = 6 * 60;   /* 06:00 */
        *sunset_min   = 18 * 60;  /* 18:00 */
        return;
    }

    int doy = ti.tm_yday;  /* 0-365 */
    double lat = DEFAULT_LAT;

    /* Solar declination */
    double decl = 23.45 * sin(to_rad(360.0 / 365.0 * (284 + doy)));

    /* Hour angle at sunrise/sunset */
    double ha = acos(-tan(to_rad(lat)) * tan(to_rad(decl)));
    double ha_hr = to_deg(ha) / 15.0;  /* hours from noon */

    double noon = 12.0 + (DEFAULT_LON - 120.0) / 15.0;  /* Beijing is UTC+8 (120E) */

    *sunrise_min = (int)((noon - ha_hr) * 60);
    *sunset_min  = (int)((noon + ha_hr) * 60);
}

int ui_theme_sunrise_min(void) {
    int sr, ss;
    calc_sun_times(&sr, &ss);
    return sr;
}

int ui_theme_sunset_min(void) {
    int sr, ss;
    calc_sun_times(&sr, &ss);
    return ss;
}

/* Public: call once WiFi is connected */
void ui_theme_try_sntp(void)
{
    /* Only schedule on lwIP task — safe to call post WiFi-connect */
    sntp_init_once();
}

/* ── Dark palette ────────────────────────────────────────────────── */
#define DARK_BG             0x0f0f23
#define DARK_SURFACE         0x1a1a2e
#define DARK_SURFACE_LIGHT   0x222240
#define DARK_TEXT            0xFFFFFF
#define DARK_TEXT_SECONDARY  0xA0A0B0
#define DARK_TEXT_MUTED      0x6B6B80

/* ── Light palette ────────────────────────────────────────────────── */
#define LIGHT_BG             0xF5F6FA
#define LIGHT_SURFACE         0xFFFFFF
#define LIGHT_SURFACE_LIGHT   0xE8EAF0
#define LIGHT_TEXT            0x1a1a2e
#define LIGHT_TEXT_SECONDARY  0x555570
#define LIGHT_TEXT_MUTED      0x9090A0

/* ── Shared style objects ────────────────────────────────────────── */
static lv_style_t st_card, st_bg, st_title;
static lv_style_t st_val_lg, st_val_md, st_label, st_cap;
static lv_style_t st_btn_primary, st_btn_ghost, st_btn_icon, st_slider;
static lv_style_t st_pressed;
static lv_style_t st_hairline;   /* flat dividers — refreshed on theme change */

/* ── Color getters ───────────────────────────────────────────────── */
static bool is_dark(void) {
    if (g_theme_mode == UI_THEME_AUTO) {
        time_t now = time(NULL);
        struct tm ti;
        localtime_r(&now, &ti);
        /* Before NTP sync, year is 1970 → default to dark to avoid white flash */
        if (ti.tm_year < 124) return true;  /* tm_year is years since 1900 */
        int sr, ss;
        calc_sun_times(&sr, &ss);
        int now_min = ti.tm_hour * 60 + ti.tm_min;
        return (now_min < sr || now_min >= ss);
    }
    return (g_theme_mode == UI_THEME_DARK);
}

lv_color_t ui_color_bg(void) {
    return lv_color_hex(is_dark() ? DARK_BG : LIGHT_BG);
}
lv_color_t ui_color_surface(void) {
    return lv_color_hex(is_dark() ? DARK_SURFACE : LIGHT_SURFACE);
}
lv_color_t ui_color_surface_light(void) {
    return lv_color_hex(is_dark() ? DARK_SURFACE_LIGHT : LIGHT_SURFACE_LIGHT);
}
lv_color_t ui_color_text(void) {
    return lv_color_hex(is_dark() ? DARK_TEXT : LIGHT_TEXT);
}
lv_color_t ui_color_text_secondary(void) {
    return lv_color_hex(is_dark() ? DARK_TEXT_SECONDARY : LIGHT_TEXT_SECONDARY);
}
lv_color_t ui_color_text_muted(void) {
    return lv_color_hex(is_dark() ? DARK_TEXT_MUTED : LIGHT_TEXT_MUTED);
}

/* ── Theme API ────────────────────────────────────────────────────── */
bool ui_theme_is_dark(void) { return is_dark(); }

ui_theme_mode_t ui_theme_get(void) { return g_theme_mode; }

const char *ui_theme_mode_label(void) {
    switch (g_theme_mode) {
        case UI_THEME_DARK:  return ui_lang_str(TXT_THEME_DARK);
        case UI_THEME_LIGHT: return ui_lang_str(TXT_THEME_LIGHT);
        case UI_THEME_AUTO:  return ui_lang_str(TXT_THEME_AUTO);
    }
    return "?";
}

static void update_shared_styles(void)
{
    lv_color_t bg     = ui_color_bg();
    lv_color_t surf   = ui_color_surface();
    lv_color_t text   = ui_color_text();
    lv_color_t text_s = ui_color_text_secondary();
    lv_color_t text_m = ui_color_text_muted();

    /* Card */
    lv_style_set_bg_color(&st_card, surf);
    /* BG */
    lv_style_set_bg_color(&st_bg, bg);
    /* Title */
    lv_style_set_text_color(&st_title, text);
    /* Value large */
    lv_style_set_text_color(&st_val_lg, text);
    /* Value medium */
    lv_style_set_text_color(&st_val_md, text);
    /* Label */
    lv_style_set_text_color(&st_label, text_s);
    /* Caption */
    lv_style_set_text_color(&st_cap, text_m);
    /* Ghost button */
    lv_style_set_text_color(&st_btn_ghost, UI_ACCENT);
    lv_style_set_border_color(&st_btn_ghost, UI_ACCENT);
    /* Icon button */
    lv_style_set_bg_color(&st_btn_icon, surf);
    /* Slider track */
    lv_style_set_bg_color(&st_slider, ui_color_surface_light());
    /* Hairline dividers */
    lv_style_set_bg_color(&st_hairline, ui_color_surface_light());
}

/* ── Auto-theme event scheduler ─────────────────────────────────────── */

/* Calculate seconds until the next sunrise or sunset event.
 * Returns -1 if system time is not yet valid (NTP not synced). */
static int seconds_until_next_event(void)
{
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    if (ti.tm_year + 1900 < 2025) return -1;

    int sr, ss;
    calc_sun_times(&sr, &ss);
    int now_min = ti.tm_hour * 60 + ti.tm_min;
    int now_sec = ti.tm_sec;

    /* Which event comes next?
     * Before sunrise  → next is sunrise today
     * Between sr and ss → next is sunset today
     * After sunset     → next is sunrise tomorrow */
    int next_min;
    if (now_min < sr)       next_min = sr;
    else if (now_min < ss)  next_min = ss;
    else                    next_min = sr + 24 * 60;

    return (next_min - now_min) * 60 - now_sec;
}

/* Apply current theme and schedule the next sunrise/sunset event. */
static void apply_and_schedule(void);

static void auto_event_cb(lv_timer_t *t)
{
    g_auto_timer = NULL;
    apply_and_schedule();
}

/* 5s poll timer — waits for SNTP to deliver valid time, then starts the
 * real auto-theme scheduler. */
static void poll_sntp_cb(lv_timer_t *t)
{
    sntp_init_once();

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year + 1900 >= 2025 || s_time_synced) {
        s_time_synced = true;
        ESP_LOGI(TAG, "SNTP time valid, starting auto-theme scheduler");
        g_poll_timer = NULL;
        lv_timer_delete(t);
        apply_and_schedule();
    }
}

static void apply_and_schedule(void)
{
    bool cur = is_dark();
    update_shared_styles();
    ui_manager_apply_theme();
    ESP_LOGI(TAG, "auto theme: %s", cur ? "dark" : "light");

    /* Delete any pending one-shot before creating the next one */
    if (g_auto_timer) {
        lv_timer_delete(g_auto_timer);
        g_auto_timer = NULL;
    }

    int secs = seconds_until_next_event();
    if (secs < 0) {
        /* Time became invalid — restart poll */
        ESP_LOGW(TAG, "time invalid, restarting SNTP poll");
        if (!g_poll_timer) g_poll_timer = lv_timer_create(poll_sntp_cb, 5000, NULL);
        return;
    }

    /* Clamp to at least 2s to avoid tight loops */
    if (secs < 2) secs = 2;

    g_auto_timer = lv_timer_create(auto_event_cb, (uint32_t)secs * 1000, NULL);
    lv_timer_set_repeat_count(g_auto_timer, 1);
    ESP_LOGI(TAG, "next auto event in %d sec (%d min)", secs, secs / 60);
}

/* ── Start / stop auto-theme scheduling ─────────────────────────────── */

static void start_auto_theme(void)
{
    if (g_auto_timer) { lv_timer_delete(g_auto_timer); g_auto_timer = NULL; }
    if (g_poll_timer) { lv_timer_delete(g_poll_timer); g_poll_timer = NULL; }

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year + 1900 >= 2025 || s_time_synced) {
        s_time_synced = true;
        apply_and_schedule();
    } else {
        g_poll_timer = lv_timer_create(poll_sntp_cb, 5000, NULL);
        ESP_LOGI(TAG, "auto theme: waiting for SNTP sync...");
    }
}

static void stop_auto_theme(void)
{
    if (g_auto_timer) { lv_timer_delete(g_auto_timer); g_auto_timer = NULL; }
    if (g_poll_timer) { lv_timer_delete(g_poll_timer); g_poll_timer = NULL; }
}

void ui_theme_set(ui_theme_mode_t mode)
{
    g_theme_mode = mode;
    update_shared_styles();

    /* Refresh active screen background */
    lv_obj_t *scr = lv_screen_active();
    if (scr) {
        lv_obj_set_style_bg_color(scr, ui_color_bg(), LV_PART_MAIN);
        lv_obj_invalidate(scr);
    }

    /* Persist to NVS */
    ESP_LOGI(TAG, "theme: opening NVS...");
    nvs_handle_t handle;
    esp_err_t nvs_err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_LOGI(TAG, "theme: nvs_open=%d", nvs_err);
    if (nvs_err == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_THEME, (uint8_t)mode);
        ESP_LOGI(TAG, "theme: committing...");
        nvs_commit(handle);
        ESP_LOGI(TAG, "theme: commit done");
        nvs_close(handle);
    }

    /* Manage auto-switch timer */
    if (mode == UI_THEME_AUTO) {
        start_auto_theme();
    } else {
        stop_auto_theme();
    }

    ESP_LOGI(TAG, "theme switched to: %s", ui_theme_mode_label());
}

void ui_theme_lang_update(void)
{
    /* Labels use ui_lang_str() directly — nothing cached to refresh */
}

/* ── Init ─────────────────────────────────────────────────────────── */
void ui_theme_init(void)
{
    /* Beijing timezone (UTC+8) — must be set before localtime_r */
    setenv("TZ", "CST-8", 1);
    tzset();

    apple_anim_init();

    /* ── Card ───────────────────────────────────────────────────── */
    lv_style_init(&st_card);
    lv_style_set_bg_color(&st_card, ui_color_surface());
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_radius(&st_card, UI_RADIUS_CARD);
    lv_style_set_border_width(&st_card, 0);
    lv_style_set_pad_all(&st_card, 16);

    /* ── Background ─────────────────────────────────────────────── */
    lv_style_init(&st_bg);
    lv_style_set_bg_color(&st_bg, ui_color_bg());
    lv_style_set_bg_opa(&st_bg, LV_OPA_COVER);
    lv_style_set_border_width(&st_bg, 0);
    lv_style_set_pad_all(&st_bg, 0);

    /* ── Title ──────────────────────────────────────────────────── */
    lv_style_init(&st_title);
    lv_style_set_text_color(&st_title, ui_color_text());
    lv_style_set_text_font(&st_title, ui_font_text());

    /* ── Value large ────────────────────────────────────────────── */
    lv_style_init(&st_val_lg);
    lv_style_set_text_color(&st_val_lg, ui_color_text());
    lv_style_set_text_font(&st_val_lg, &lv_font_montserrat_48);

    /* ── Value medium ───────────────────────────────────────────── */
    lv_style_init(&st_val_md);
    lv_style_set_text_color(&st_val_md, ui_color_text());
    lv_style_set_text_font(&st_val_md, &lv_font_montserrat_36);

    /* ── Label ──────────────────────────────────────────────────── */
    lv_style_init(&st_label);
    lv_style_set_text_color(&st_label, ui_color_text_secondary());
    lv_style_set_text_font(&st_label, ui_font_text());

    /* ── Caption ────────────────────────────────────────────────── */
    lv_style_init(&st_cap);
    lv_style_set_text_color(&st_cap, ui_color_text_muted());
    lv_style_set_text_font(&st_cap, ui_font_text());

    /* ── Primary button ─────────────────────────────────────────── */
    lv_style_init(&st_btn_primary);
    lv_style_set_bg_color(&st_btn_primary, UI_ACCENT);
    lv_style_set_bg_opa(&st_btn_primary, LV_OPA_COVER);
    lv_style_set_radius(&st_btn_primary, UI_RADIUS_BTN);
    lv_style_set_border_width(&st_btn_primary, 0);
    lv_style_set_shadow_width(&st_btn_primary, 0);
    lv_style_set_text_color(&st_btn_primary, lv_color_white());
    lv_style_set_text_font(&st_btn_primary, ui_font_text());
    lv_style_set_pad_ver(&st_btn_primary, 14);
    lv_style_set_pad_hor(&st_btn_primary, 36);

    /* ── Ghost button ───────────────────────────────────────────── */
    lv_style_init(&st_btn_ghost);
    lv_style_set_bg_opa(&st_btn_ghost, LV_OPA_TRANSP);
    lv_style_set_radius(&st_btn_ghost, UI_RADIUS_BTN);
    lv_style_set_border_width(&st_btn_ghost, 1);
    lv_style_set_border_color(&st_btn_ghost, UI_ACCENT);
    lv_style_set_border_opa(&st_btn_ghost, LV_OPA_COVER);
    lv_style_set_shadow_width(&st_btn_ghost, 0);
    lv_style_set_text_color(&st_btn_ghost, UI_ACCENT);
    lv_style_set_text_font(&st_btn_ghost, ui_font_text());
    lv_style_set_pad_ver(&st_btn_ghost, 10);
    lv_style_set_pad_hor(&st_btn_ghost, 24);

    /* ── Icon button ────────────────────────────────────────────── */
    lv_style_init(&st_btn_icon);
    lv_style_set_bg_color(&st_btn_icon, ui_color_surface());
    lv_style_set_bg_opa(&st_btn_icon, LV_OPA_COVER);
    lv_style_set_radius(&st_btn_icon, UI_RADIUS_BTN);
    lv_style_set_border_width(&st_btn_icon, 0);
    lv_style_set_shadow_width(&st_btn_icon, 0);

    /* ── Slider ─────────────────────────────────────────────────── */
    lv_style_init(&st_slider);
    lv_style_set_bg_color(&st_slider, ui_color_surface_light());
    lv_style_set_bg_opa(&st_slider, LV_OPA_COVER);
    lv_style_set_radius(&st_slider, 18);
    lv_style_set_pad_ver(&st_slider, 4);

    /* ── Hairline divider (flat iOS separators) ────────────────── */
    lv_style_init(&st_hairline);
    lv_style_set_bg_color(&st_hairline, ui_color_surface_light());
    lv_style_set_bg_opa(&st_hairline, LV_OPA_COVER);
    lv_style_set_border_width(&st_hairline, 0);
    lv_style_set_radius(&st_hairline, 0);

    /* ── Pressed state (no animation) ──────────────────────────── */
    lv_style_init(&st_pressed);

    /* ── Global scrollbar: hidden (no knob) ────────────────────────── */
    lv_obj_set_style_width(lv_screen_active(), 0, LV_PART_SCROLLBAR);

    /* Load saved theme from NVS (default LIGHT) */
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t val = UI_THEME_LIGHT;
        if (nvs_get_u8(handle, NVS_KEY_THEME, &val) == ESP_OK && val <= UI_THEME_AUTO) {
            g_theme_mode = (ui_theme_mode_t)val;
            update_shared_styles();
        }
        nvs_close(handle);
    }
    if (g_theme_mode == UI_THEME_AUTO) {
        start_auto_theme();
    }

    ESP_LOGI(TAG, "theme init done (%s)", ui_theme_mode_label());
}

/* ── Style getters ───────────────────────────────────────────────── */
lv_style_t *ui_style_title(void)       { return &st_title; }
lv_style_t *ui_style_value_lg(void)    { return &st_val_lg; }
lv_style_t *ui_style_value_md(void)    { return &st_val_md; }
lv_style_t *ui_style_label(void)       { return &st_label; }
lv_style_t *ui_style_caption(void)     { return &st_cap; }
lv_style_t *ui_style_btn_primary(void) { return &st_btn_primary; }
lv_style_t *ui_style_btn_ghost(void)   { return &st_btn_ghost; }
lv_style_t *ui_style_btn_icon(void)    { return &st_btn_icon; }
lv_style_t *ui_style_slider(void)      { return &st_slider; }
lv_style_t *ui_style_pressed(void)     { return &st_pressed; }

lv_style_t *ui_style_hairline(void)    { return &st_hairline; }

lv_style_t *lg_style_card(void)        { return &st_card; }
lv_style_t *lg_style_bg(void)          { return &st_bg; }
lv_style_t *lg_style_value_lg(void)    { return &st_val_lg; }
lv_style_t *lg_style_value_md(void)    { return &st_val_md; }
lv_style_t *lg_style_caption(void)     { return &st_cap; }

lv_style_t *ui_style_glass(void)        { return &st_card; }
lv_style_t *ui_style_glass_accent(void) { return &st_card; }

/* ── AQI helpers (TVOC) ──────────────────────────────────────────── */
lv_color_t ui_aqi_color(uint16_t tvoc_ugm3)
{
    if (tvoc_ugm3 <= 400)  return UI_GOOD;
    if (tvoc_ugm3 <= 1200)  return UI_MODERATE;
    if (tvoc_ugm3 <= 2400)  return UI_UNHEALTHY;
    if (tvoc_ugm3 <= 4000) return UI_BAD;
    return UI_HAZARDOUS;
}

const char *ui_aqi_label(uint16_t tvoc_ugm3)
{
    if (tvoc_ugm3 <= 400)  return ui_lang_str(TXT_AQI_EXCELLENT);
    if (tvoc_ugm3 <= 1200)  return ui_lang_str(TXT_AQI_GOOD);
    if (tvoc_ugm3 <= 2400)  return ui_lang_str(TXT_AQI_MODERATE);
    if (tvoc_ugm3 <= 4000) return ui_lang_str(TXT_AQI_POOR);
    return ui_lang_str(TXT_AQI_HAZARDOUS);
}

