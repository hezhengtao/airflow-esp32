#include "ui_screen_power.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_apple_anim.h"
#include "ui_manager.h"
#include "board.h"
#include "app_controller.h"
#include "holiday_client.h"
#include "esp_log.h"
#include "settings.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

#define TAG "power"

/* ── Schedule state ────────────────────────────────────────────────── */

static uint8_t  g_off_h = 23, g_off_m = 0;
static uint8_t  g_on_h = 7,  g_on_m = 0;
static int      g_off_day = 0;  /* 0=今天 1=明天 2=后天 3=每天 4=工作日 5=周末 6-12=周一~日 13=自定义 */
static int      g_on_day = 0;
static uint8_t  g_off_day_mask = 0;  /* bit0=Mon..bit6=Sun, used when mode==13 */
static uint8_t  g_on_day_mask = 0;
static uint32_t g_off_date = 0; /* packed Y-M-D: (year<<16)|(month<<8)|day */
static uint32_t g_on_date = 0;
static bool     g_sched_off_en = false;
static bool     g_sched_on_en = false;

static lv_obj_t *g_off_h_rol, *g_off_m_rol, *g_off_day_rol;
static lv_obj_t *g_on_h_rol,  *g_on_m_rol,  *g_on_day_rol;
static lv_obj_t *g_off_sw = NULL, *g_on_sw = NULL;   /* per-schedule enable */
static lv_obj_t *g_screen_off_lbl = NULL;
static lv_obj_t *g_shutdown_lbl = NULL;

/* Pre-built option strings */
static char g_hour_opts[128];   /* "00\n01\n...\n23" */
static char g_min_opts[192];    /* "00\n01\n...\n59" */
static char g_day_opts[256];    /* 13 day-mode options */

/* ── Helpers ────────────────────────────────────────────────────────── */

static void build_opts(void)
{
    int pos = 0;
    for (int i = 0; i < 24; i++)
        pos += snprintf(g_hour_opts + pos, sizeof(g_hour_opts) - pos,
                        "%s%02d", i > 0 ? "\n" : "", i);
    pos = 0;
    for (int i = 0; i < 60; i++)
        pos += snprintf(g_min_opts + pos, sizeof(g_min_opts) - pos,
                        "%s%02d", i > 0 ? "\n" : "", i);
    snprintf(g_day_opts, sizeof(g_day_opts),
             "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s",
             ui_lang_str(TXT_TODAY), ui_lang_str(TXT_TOMORROW),
             ui_lang_str(TXT_DAY_AFTER), ui_lang_str(TXT_EVERY_DAY),
             ui_lang_str(TXT_WEEKDAYS), ui_lang_str(TXT_WEEKENDS),
             ui_lang_str(TXT_MON), ui_lang_str(TXT_TUE),
             ui_lang_str(TXT_WED), ui_lang_str(TXT_THU),
             ui_lang_str(TXT_FRI), ui_lang_str(TXT_SAT),
             ui_lang_str(TXT_SUN));
}

/* ── Roller factory ─────────────────────────────────────────────────── */

static lv_obj_t *make_roller(lv_obj_t *parent, int x, int y, int w,
                             const char *opts, int sel, lv_event_cb_t cb)
{
    lv_obj_t *rol = lv_roller_create(parent);
    lv_roller_set_options(rol, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rol, 5);
    lv_roller_set_selected(rol, sel, LV_ANIM_OFF);
    lv_obj_set_size(rol, w, 92);
    lv_obj_set_pos(rol, x, y);
    lv_obj_set_style_text_font(rol, ui_font_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(rol, ui_font_text(), LV_PART_SELECTED);
    /* Match card bg to mask partial rows at top/bottom */
    lv_color_t card_bg = lv_obj_get_style_bg_color(parent, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rol, card_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rol, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rol, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_color(rol, UI_TEXT_MUTED, LV_PART_MAIN);
    lv_obj_set_style_text_color(rol, UI_TEXT, LV_PART_SELECTED);
    lv_obj_set_style_text_align(rol, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(rol, LV_TEXT_ALIGN_CENTER, LV_PART_SELECTED);
    lv_obj_set_style_border_width(rol, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(rol, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(rol, UI_RADIUS_SM, LV_PART_MAIN);
    lv_obj_add_event_cb(rol, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return rol;
}

/* Auto-save: any roller/switch change persists immediately (no Save button) */
static void sched_save_off_to_nvs(void);
static void sched_save_on_to_nvs(void);

/* Roller changes update RAM immediately; persist to NVS only when that
 * schedule is enabled (avoids flash writes while adjusting a disabled one —
 * the switch's own callback persists everything when it's turned on). */
static void off_h_cb(lv_event_t *e) { g_off_h = (uint8_t)lv_roller_get_selected(g_off_h_rol); if (g_sched_off_en) sched_save_off_to_nvs(); }
static void off_m_cb(lv_event_t *e) { g_off_m = (uint8_t)lv_roller_get_selected(g_off_m_rol); if (g_sched_off_en) sched_save_off_to_nvs(); }
static void off_day_cb(lv_event_t *e) {
    g_off_day = lv_roller_get_selected(g_off_day_rol);
    if (g_sched_off_en) sched_save_off_to_nvs();
}
static void on_h_cb(lv_event_t *e)  { g_on_h  = (uint8_t)lv_roller_get_selected(g_on_h_rol); if (g_sched_on_en) sched_save_on_to_nvs(); }
static void on_m_cb(lv_event_t *e)  { g_on_m  = (uint8_t)lv_roller_get_selected(g_on_m_rol); if (g_sched_on_en) sched_save_on_to_nvs(); }
static void on_day_cb(lv_event_t *e) {
    g_on_day = lv_roller_get_selected(g_on_day_rol);
    if (g_sched_on_en) sched_save_on_to_nvs();
}

static void off_sw_cb(lv_event_t *e)
{
    g_sched_off_en = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sched_save_off_to_nvs();
    ESP_LOGI(TAG, "Off schedule %s: %02d:%02d", g_sched_off_en ? "ON" : "OFF", g_off_h, g_off_m);
}
static void on_sw_cb(lv_event_t *e)
{
    g_sched_on_en = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sched_save_on_to_nvs();
    ESP_LOGI(TAG, "On schedule %s: %02d:%02d", g_sched_on_en ? "ON" : "OFF", g_on_h, g_on_m);
}

/* ── Date helpers ──────────────────────────────────────────────────── */

static uint32_t pack_date(int y, int m, int d) {
    return ((uint32_t)y << 16) | ((uint32_t)m << 8) | (uint32_t)d;
}

/* Compute absolute target date: today + day_offset days */
static uint32_t make_target_date(int day_offset)
{
    time_t now = time(NULL);
    time_t target = now + day_offset * 86400;
    struct tm ti;
    localtime_r(&target, &ti);
    return pack_date(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
}

/* Check if day_mode matches current date/weekday.
 * wday: 0=Sun 1=Mon … 6=Sat.  day_mask: used for mode 13 (bit0=Mon…bit6=Sun). */
static bool day_matches(int day_mode, uint32_t target_date, uint32_t today_packed,
                        int tm_wday, int month, int day, uint8_t day_mask)
{
    switch (day_mode) {
    case 0: case 1: case 2:  /* 今天/明天/后天 — absolute date */
        return target_date == today_packed;
    case 3:  /* 每天 */
        return true;
    case 4:  /* 工作日 — 含调休补班 */
        return holiday_is_workday(month, day, tm_wday);
    case 5:  /* 周末 — 含法定假日，排除调休 */
        return holiday_is_rest_day(month, day, tm_wday);
    case 6:  return tm_wday == 1;  /* 周一 */
    case 7:  return tm_wday == 2;  /* 周二 */
    case 8:  return tm_wday == 3;  /* 周三 */
    case 9:  return tm_wday == 4;  /* 周四 */
    case 10: return tm_wday == 5;  /* 周五 */
    case 11: return tm_wday == 6;  /* 周六 */
    case 12: return tm_wday == 0;  /* 周日 */
    case 13: /* 自定义多选 — bit0=Mon…bit6=Sun */
        return (day_mask & (1 << (tm_wday == 0 ? 6 : tm_wday - 1))) != 0;
    }
    return false;
}

/* ── NVS ────────────────────────────────────────────────────────────── */

static void sched_save_off_to_nvs(void)
{
    if (g_off_day <= 2) g_off_date = make_target_date(g_off_day);
    settings_save_u8(NVS_KEY_SCHED_OFF_EN, g_sched_off_en ? 1 : 0);
    settings_save_u8(NVS_KEY_SCHED_OFF_H, g_off_h);
    settings_save_u8(NVS_KEY_SCHED_OFF_M, g_off_m);
    settings_save_u8(NVS_KEY_SCHED_OFF_DAY, (uint8_t)g_off_day);
    settings_save_u32(NVS_KEY_SCHED_OFF_DATE, g_off_date);
    settings_save_u8(NVS_KEY_SCHED_OFF_MASK, g_off_day_mask);
    settings_commit();
}

static void sched_save_on_to_nvs(void)
{
    if (g_on_day <= 2) g_on_date = make_target_date(g_on_day);
    settings_save_u8(NVS_KEY_SCHED_ON_EN, g_sched_on_en ? 1 : 0);
    settings_save_u8(NVS_KEY_SCHED_ON_H, g_on_h);
    settings_save_u8(NVS_KEY_SCHED_ON_M, g_on_m);
    settings_save_u8(NVS_KEY_SCHED_ON_DAY, (uint8_t)g_on_day);
    settings_save_u32(NVS_KEY_SCHED_ON_DATE, g_on_date);
    settings_save_u8(NVS_KEY_SCHED_ON_MASK, g_on_day_mask);
    settings_commit();
}

static void sched_load_from_nvs(void)
{
    uint8_t v;
    uint32_t u32;
    if (settings_get_u8(NVS_KEY_SCHED_EN, &v) == ESP_OK) { g_sched_off_en = (v != 0); g_sched_on_en = (v != 0); }
    if (settings_get_u8(NVS_KEY_SCHED_OFF_EN, &v) == ESP_OK) g_sched_off_en = (v != 0);
    if (settings_get_u8(NVS_KEY_SCHED_ON_EN, &v) == ESP_OK) g_sched_on_en = (v != 0);
    if (settings_get_u8(NVS_KEY_SCHED_OFF_H, &v) == ESP_OK) g_off_h = v;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_M, &v) == ESP_OK) g_off_m = v;
    if (settings_get_u8(NVS_KEY_SCHED_ON_H, &v) == ESP_OK) g_on_h = v;
    if (settings_get_u8(NVS_KEY_SCHED_ON_M, &v) == ESP_OK) g_on_m = v;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_DAY, &v) == ESP_OK) g_off_day = v;
    if (settings_get_u8(NVS_KEY_SCHED_ON_DAY, &v) == ESP_OK) g_on_day = v;
    if (settings_get_u8(NVS_KEY_SCHED_OFF_MASK, &v) == ESP_OK) g_off_day_mask = v;
    if (settings_get_u8(NVS_KEY_SCHED_ON_MASK, &v) == ESP_OK) g_on_day_mask = v;
    if (settings_get_u32(NVS_KEY_SCHED_OFF_DATE, &u32) == ESP_OK) {
        g_off_date = u32;
    } else {
        g_off_date = make_target_date(g_off_day);
    }
    if (settings_get_u32(NVS_KEY_SCHED_ON_DATE, &u32) == ESP_OK) {
        g_on_date = u32;
    } else {
        g_on_date = make_target_date(g_on_day);
    }
}

/* ── Save button → enable schedule ─────────────────────────────────── */

/* Schedule enabling/saving is now automatic on any switch/roller change
 * (off_sw_cb / on_sw_cb / *_h_cb / *_m_cb / *_day_cb) — no Save buttons. */

/* ── Manual power actions ──────────────────────────────────────────── */

static void screen_off_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    ui_manager_screen_off();
}

static void shutdown_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    ui_manager_shutdown();
}

static void btn_press_cb(lv_event_t *e)  { apple_press_effect(lv_event_get_target(e)); }
static void btn_release_cb(lv_event_t *e) { apple_release_effect(lv_event_get_target(e)); }

bool ui_screen_power_is_schedule_active(void)
{
    return g_sched_off_en || g_sched_on_en;
}

void ui_screen_power_sync_schedule(void)
{
    sched_load_from_nvs();
    /* Recompute absolute dates for one-shot modes */
    if (g_off_day <= 2) {
        g_off_date = make_target_date(g_off_day);
        settings_save_u32(NVS_KEY_SCHED_OFF_DATE, g_off_date);
    }
    if (g_on_day <= 2) {
        g_on_date = make_target_date(g_on_day);
        settings_save_u32(NVS_KEY_SCHED_ON_DATE, g_on_date);
    }
    settings_commit();
    ESP_LOGI(TAG, "Schedule synced: off_en=%d off=%02d:%02d(mode=%d,mask=0x%02x) on_en=%d on=%02d:%02d(mode=%d,mask=0x%02x)",
             g_sched_off_en, g_off_h, g_off_m, g_off_day, g_off_day_mask,
             g_sched_on_en, g_on_h, g_on_m, g_on_day, g_on_day_mask);
}

/* ── Schedule check timer (runs every 30s) ──────────────────────────── */

static int  g_last_trigger_min = -1;  /* de-bounce: prevent double-fire within same minute */
static int  g_shutdown_log_cnt = 0;   /* heartbeat counter during shutdown */

static void sched_check_timer_cb(lv_timer_t *t)
{
    if (!g_sched_off_en && !g_sched_on_en) return;

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year + 1900 < 2025) return;

    uint32_t today = pack_date(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    int cur_min = ti.tm_hour * 60 + ti.tm_min;
    int wday = ti.tm_wday;  /* 0=Sun 1=Mon ... 6=Sat */
    bool is_off = app_controller_is_shutdown();

    /* Debug: log state every minute at :00 second boundary */
    static int g_last_log_min = -1;
    if (cur_min != g_last_log_min) {
        g_last_log_min = cur_min;
        int on_min = g_on_h * 60 + g_on_m;
        bool on_day_match = day_matches(g_on_day, g_on_date, today, wday,
                                        ti.tm_mon + 1, ti.tm_mday, g_on_day_mask);
        ESP_LOGI(TAG, "sched tick: time=%02d:%02d wday=%d is_off=%d "
                 "off_en=%d on_en=%d on_target=%02d:%02d on_day=%d on_date=%lu "
                 "on_day_match=%d on_min_match=%d",
                 ti.tm_hour, ti.tm_min, wday, is_off,
                 g_sched_off_en, g_sched_on_en, g_on_h, g_on_m, g_on_day,
                 (unsigned long)g_on_date, on_day_match, cur_min == on_min);
    }

    /* Heartbeat: confirm timer is alive during shutdown (every ~5 min) */
    if (is_off) {
        g_shutdown_log_cnt++;
        if (g_shutdown_log_cnt % 10 == 0) {
            ESP_LOGI(TAG, "sched alive in shutdown (min=%d on=%02d:%02d mode=%d)",
                     cur_min, g_on_h, g_on_m, g_on_day);
        }
    } else {
        g_shutdown_log_cnt = 0;
    }

    if (!is_off && g_sched_off_en) {
        int off_min = g_off_h * 60 + g_off_m;
        if (day_matches(g_off_day, g_off_date, today, wday,
                        ti.tm_mon + 1, ti.tm_mday, g_off_day_mask) && cur_min == off_min) {
            if (cur_min == g_last_trigger_min) return;
            g_last_trigger_min = cur_min;
            ESP_LOGI(TAG, "Scheduled shutdown at %02d:%02d (mode=%d)",
                     ti.tm_hour, ti.tm_min, g_off_day);
            ui_manager_shutdown();
            if (g_off_day <= 2) {
                g_sched_off_en = false;
                settings_save_u8(NVS_KEY_SCHED_OFF_EN, 0);
                settings_commit();
                ESP_LOGI(TAG, "One-shot shutdown done, off schedule disabled");
            }
        }
    } else if (is_off && g_sched_on_en) {
        int on_min = g_on_h * 60 + g_on_m;
        if (day_matches(g_on_day, g_on_date, today, wday,
                        ti.tm_mon + 1, ti.tm_mday, g_on_day_mask) && cur_min == on_min) {
            if (cur_min == g_last_trigger_min) return;
            g_last_trigger_min = cur_min;
            ESP_LOGI(TAG, "Scheduled wake at %02d:%02d (mode=%d) — calling ui_manager_wake()",
                     ti.tm_hour, ti.tm_min, g_on_day);
            ui_manager_wake();
            if (g_on_day <= 2) {
                g_sched_on_en = false;
                settings_save_u8(NVS_KEY_SCHED_ON_EN, 0);
                settings_commit();
                ESP_LOGI(TAG, "One-shot wake done, on schedule disabled");
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Boot-time schedule init — runs once at startup, independent of the screen.
   Ensures scheduled power on/off works even if the Power screen is never
   opened (screens are created lazily on first navigation).
   ═══════════════════════════════════════════════════════════════════════ */
void ui_screen_power_init_schedule(void)
{
    static bool inited = false;
    if (inited) return;
    inited = true;
    build_opts();
    sched_load_from_nvs();
    lv_timer_create(sched_check_timer_cb, 30000, NULL);
    ESP_LOGI(TAG, "schedule init (off_en=%d %02d:%02d  on_en=%d %02d:%02d)",
             g_sched_off_en, g_off_h, g_off_m, g_sched_on_en, g_on_h, g_on_m);
}

/* ═══════════════════════════════════════════════════════════════════════
   Create
   ═══════════════════════════════════════════════════════════════════════ */

lv_obj_t *ui_screen_power_create(lv_obj_t *parent)
{
    int pw = UI_CONTENT_W, ph = UI_CONTENT_H;  /* 728 x 480 content area */
    int card_w = pw - 24;  /* 704 */

    /* Schedule state/options + timer are initialised at boot by
     * ui_screen_power_init_schedule(); do NOT re-init here (avoids a 2nd
     * 30s timer).  g_day_opts etc. are already populated. */

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, pw, ph);
    lv_obj_set_pos(root, UI_CONTENT_X, 0);
    lv_obj_add_style(root, lg_style_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(root, LG_BG_TOP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(root, LV_DIR_NONE);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* ═══════════════════════════════════════════════════════════════════
     * Card 1: Manual Controls — two big buttons
     * ═══════════════════════════════════════════════════════════════════ */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, card_w, 152);
        lv_obj_set_pos(card, 12, 12);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        /* Content area = card_w - 2*pad = card_w - 32 */
        int content_w = card_w - 32;
        int btn_w = (content_w - 40) / 2;   /* 316 */
        int btn_h = 130;

        /* Screen Off (blue) */
        lv_obj_t *off_btn = lv_btn_create(card);
        lv_obj_set_size(off_btn, btn_w, btn_h);
        lv_obj_set_pos(off_btn, 0, 4);
        lv_obj_set_style_radius(off_btn, 20, LV_PART_MAIN);
        lv_obj_set_style_bg_color(off_btn, UI_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_width(off_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(off_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(off_btn, btn_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(off_btn, btn_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(off_btn, screen_off_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *off_icon = lv_label_create(off_btn);
        lv_label_set_text(off_icon, LV_SYMBOL_EYE_CLOSE);
        lv_obj_set_style_text_color(off_icon, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(off_icon, &lv_font_montserrat_36, LV_PART_MAIN);
        lv_obj_align(off_icon, LV_ALIGN_CENTER, 0, -24);

        lv_obj_t *off_lbl = lv_label_create(off_btn);
        g_screen_off_lbl = off_lbl;
        lv_label_set_text(off_lbl, ui_lang_str(TXT_SCREEN_OFF));
        lv_obj_set_style_text_color(off_lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(off_lbl, ui_font_cjk_28(), LV_PART_MAIN);
        lv_obj_align(off_lbl, LV_ALIGN_CENTER, 0, 24);

        /* Shutdown (red) */
        lv_obj_t *sd_btn = lv_btn_create(card);
        lv_obj_set_size(sd_btn, btn_w, btn_h);
        lv_obj_set_pos(sd_btn, btn_w + 40, 4);
        lv_obj_set_style_radius(sd_btn, 20, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sd_btn, LG_DANGER, LV_PART_MAIN);
        lv_obj_set_style_border_width(sd_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(sd_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(sd_btn, btn_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(sd_btn, btn_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(sd_btn, shutdown_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *sd_icon = lv_label_create(sd_btn);
        lv_label_set_text(sd_icon, LV_SYMBOL_POWER);
        lv_obj_set_style_text_color(sd_icon, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(sd_icon, &lv_font_montserrat_36, LV_PART_MAIN);
        lv_obj_align(sd_icon, LV_ALIGN_CENTER, 0, -24);

        lv_obj_t *sd_lbl = lv_label_create(sd_btn);
        g_shutdown_lbl = sd_lbl;
        lv_label_set_text(sd_lbl, ui_lang_str(TXT_SHUTDOWN));
        lv_obj_set_style_text_color(sd_lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(sd_lbl, ui_font_cjk_28(), LV_PART_MAIN);
        lv_obj_align(sd_lbl, LV_ALIGN_CENTER, 0, 24);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Card 2: Scheduled Power — 天→时:分, fills bottom of screen
     * ═══════════════════════════════════════════════════════════════════ */
    {
        lv_obj_t *card = lv_obj_create(root);
        int card_h = ph - 12 - 152 - 12 - 12;   /* 292 */
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_pos(card, 12, 176);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        int content_w = card_w - 32;   /* 672 */

        /* Title */
        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, ui_lang_str(TXT_SCHEDULE_TITLE));
        lv_obj_set_style_text_color(title, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(title, 0, 0);

        /* Roller cluster geometry */
        int day_w = 120, h_w = 64, m_w = 64, colon_w = 16, gap = 8;
        int rol_h = 92;
        int row1_y = 22;
        int row2_y = 142;
        /* Center the entire [label+switch + rollers] block horizontally */
        int lbl_w = 80, sw_w = 60;
        int cluster_w = lbl_w + gap + sw_w + gap*2 + day_w + gap + h_w + gap + colon_w + gap + m_w;
        int block_x = (content_w - cluster_w) / 2;
        int x_lbl = block_x;
        int x_sw  = x_lbl + lbl_w + gap;
        int x_day = x_sw + sw_w + gap;
        int x_h   = x_day + day_w + gap;
        int x_col = x_h + h_w + gap;
        int x_m   = x_col + colon_w + gap;

        /* ── Row 1: 关机定时 ───────────────────────────────── */
        lv_obj_t *off_lbl = lv_label_create(card);
        lv_label_set_text(off_lbl, ui_lang_str(TXT_SCHED_OFF_TIME));
        lv_obj_set_style_text_color(off_lbl, UI_TEXT_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(off_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(off_lbl, x_lbl, row1_y + rol_h / 2 - 14);

        g_off_sw = lv_switch_create(card);
        lv_obj_set_size(g_off_sw, 52, 28);
        lv_obj_set_pos(g_off_sw, x_sw, row1_y + rol_h / 2 - 14);
        lv_obj_set_style_bg_color(g_off_sw, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_off_sw, UI_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (g_sched_off_en) lv_obj_add_state(g_off_sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(g_off_sw, off_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

        g_off_day_rol = make_roller(card, x_day, row1_y, day_w, g_day_opts, g_off_day, off_day_cb);
        g_off_h_rol   = make_roller(card, x_h, row1_y, h_w, g_hour_opts, g_off_h, off_h_cb);
        g_off_m_rol   = make_roller(card, x_m, row1_y, m_w, g_min_opts, g_off_m, off_m_cb);

        lv_obj_t *col1 = lv_label_create(card);
        lv_label_set_text(col1, ":");
        lv_obj_set_style_text_color(col1, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(col1, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(col1, x_col, row1_y + rol_h / 2 - 14);

        /* ── Row 2: 开机定时 ───────────────────────────────── */
        lv_obj_t *on_lbl = lv_label_create(card);
        lv_label_set_text(on_lbl, ui_lang_str(TXT_SCHED_ON_TIME));
        lv_obj_set_style_text_color(on_lbl, UI_TEXT_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(on_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(on_lbl, x_lbl, row2_y + rol_h / 2 - 14);

        g_on_sw = lv_switch_create(card);
        lv_obj_set_size(g_on_sw, 52, 28);
        lv_obj_set_pos(g_on_sw, x_sw, row2_y + rol_h / 2 - 14);
        lv_obj_set_style_bg_color(g_on_sw, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_on_sw, UI_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (g_sched_on_en) lv_obj_add_state(g_on_sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(g_on_sw, on_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

        g_on_day_rol = make_roller(card, x_day, row2_y, day_w, g_day_opts, g_on_day, on_day_cb);
        g_on_h_rol   = make_roller(card, x_h, row2_y, h_w, g_hour_opts, g_on_h, on_h_cb);
        g_on_m_rol   = make_roller(card, x_m, row2_y, m_w, g_min_opts, g_on_m, on_m_cb);

        lv_obj_t *col2 = lv_label_create(card);
        lv_label_set_text(col2, ":");
        lv_obj_set_style_text_color(col2, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(col2, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(col2, x_col, row2_y + rol_h / 2 - 14);
    }

    /* (schedule timer created once in ui_screen_power_init_schedule) */
    ESP_LOGI(TAG, "power screen created (off_en=%d off=%02d:%02d on_en=%d on=%02d:%02d)",
             g_sched_off_en, g_off_h, g_off_m, g_sched_on_en, g_on_h, g_on_m);
    return root;
}

/* ── Lang / Theme update ────────────────────────────────────────────── */

void ui_screen_power_lang_update(void)
{
    if (g_screen_off_lbl)
        lv_label_set_text(g_screen_off_lbl, ui_lang_str(TXT_SCREEN_OFF));
    if (g_shutdown_lbl)
        lv_label_set_text(g_shutdown_lbl, ui_lang_str(TXT_SHUTDOWN));

    /* Rebuild day options (Chinese/English) and update rollers */
    snprintf(g_day_opts, sizeof(g_day_opts),
             "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s",
             ui_lang_str(TXT_TODAY), ui_lang_str(TXT_TOMORROW),
             ui_lang_str(TXT_DAY_AFTER), ui_lang_str(TXT_EVERY_DAY),
             ui_lang_str(TXT_WEEKDAYS), ui_lang_str(TXT_WEEKENDS),
             ui_lang_str(TXT_MON), ui_lang_str(TXT_TUE),
             ui_lang_str(TXT_WED), ui_lang_str(TXT_THU),
             ui_lang_str(TXT_FRI), ui_lang_str(TXT_SAT),
             ui_lang_str(TXT_SUN));
    if (g_off_day_rol) {
        int sel = lv_roller_get_selected(g_off_day_rol);
        lv_roller_set_options(g_off_day_rol, g_day_opts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(g_off_day_rol, sel, LV_ANIM_OFF);
    }
    if (g_on_day_rol) {
        int sel = lv_roller_get_selected(g_on_day_rol);
        lv_roller_set_options(g_on_day_rol, g_day_opts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(g_on_day_rol, sel, LV_ANIM_OFF);
    }
}

void ui_screen_power_theme_update(void)
{
    lv_obj_t *rollers[] = { g_off_h_rol, g_off_m_rol, g_off_day_rol,
                            g_on_h_rol,  g_on_m_rol,  g_on_day_rol };
    for (int i = 0; i < 6; i++) {
        if (!rollers[i]) continue;
        lv_obj_set_style_text_color(rollers[i], UI_TEXT_MUTED, LV_PART_MAIN);
        lv_obj_set_style_text_color(rollers[i], UI_TEXT, LV_PART_SELECTED);
        lv_obj_t *p = lv_obj_get_parent(rollers[i]);
        if (p) {
            lv_obj_set_style_bg_color(rollers[i],
                lv_obj_get_style_bg_color(p, LV_PART_MAIN), LV_PART_MAIN);
        }
    }
}

void ui_screen_power_publish_schedule(void) {}
void ui_screen_power_set_web_schedules(const char *json) { (void)json; }
