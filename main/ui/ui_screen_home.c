#include "ui_screen_home.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "app_controller.h"
#include "holiday_client.h"
#include "board.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>

#define TAG "home"
#define CHART_PTS 60

/* Global widgets */
lv_obj_t *ui_home_temp_label = NULL;
lv_obj_t *ui_home_tvoc_label = NULL;
lv_obj_t *ui_home_co2_label = NULL;
lv_obj_t *ui_home_ch2o_label = NULL;
lv_obj_t *ui_home_fan_pct_label = NULL;
lv_obj_t *ui_home_fan_rpm_label = NULL;
lv_obj_t *ui_home_fan_slider = NULL;
lv_obj_t *ui_home_fan_btn = NULL;
lv_obj_t *ui_home_fan_btn_label = NULL;

static lv_obj_t *g_temp_chart = NULL;
static lv_obj_t *g_tvoc_chart = NULL;
static lv_obj_t *g_co2_chart = NULL;
static lv_obj_t *g_ch2o_chart = NULL;
static lv_obj_t *g_gas_title = NULL;
static lv_obj_t *g_fan_title = NULL;
static lv_obj_t *g_gas_card = NULL;
static lv_obj_t *g_fan_card = NULL;
static lv_obj_t *g_holiday_label = NULL;
static lv_obj_t *g_temp_title = NULL;
static lv_obj_t *g_gas_rows[3] = {0};
static bool g_fan_on = false;
static uint8_t g_last_speed = 0;
static uint32_t g_slider_last_update_ms = 0;

/* ── Per-chart ring buffers for Y-axis auto-scale ─────────────────── */
static int g_temp_ring[CHART_PTS], g_tvoc_ring[CHART_PTS];
static int g_co2_ring[CHART_PTS],  g_ch2o_ring[CHART_PTS];
static int *g_rings[4] = {g_temp_ring, g_tvoc_ring, g_co2_ring, g_ch2o_ring};
static uint32_t g_ring_cnt[4] = {0};

static void autoscale(lv_obj_t *c, int idx)
{
    int *p = g_rings[idx]; uint32_t n = g_ring_cnt[idx];
    if (!c || n < 1) return;
    int s = (n < CHART_PTS) ? (int)n : CHART_PTS;
    int lo = p[0], hi = p[0];
    for (int i = 1; i < s; i++) {
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    int span = hi - lo;
    if (span < 2) { int mid = (lo + hi) / 2; lo = mid - 1; hi = mid + 1; span = 2; }
    int pad = span / 4; if (pad < 1) pad = 1;
    int y0 = lo - pad; if (y0 < 0) y0 = 0;
    int y1 = hi + pad; if (y1 <= y0) y1 = y0 + 1;
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, y0, y1);
}

/* ── Sparkline helper ─────────────────────────────────────────────── */
static lv_obj_t *make_chart(lv_obj_t *parent, int x, int y, int w, int h,
                             int ylo, int yhi)
{
    lv_obj_t *c = lv_chart_create(parent);
    lv_chart_set_type(c, LV_CHART_TYPE_LINE);
    lv_obj_set_size(c, w, h);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_chart_set_point_count(c, CHART_PTS);
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, ylo, yhi);
    lv_chart_set_div_line_count(c, 0, 0);
    lv_chart_add_series(c, UI_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(c, 0, 0, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 2, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(c, UI_ACCENT, LV_PART_INDICATOR);
    return c;
}

/* ── Severity color vs web alarm threshold ─────────────────────────── */
static lv_color_t sev_color(int val, int thr, int def)
{
    int t = (thr > 0) ? thr : def;
    if (t <= 0) return UI_GOOD;
    float r = (float)val / (float)t;
    if (r < 0.50f) return UI_GOOD;
    if (r < 0.80f) return UI_MODERATE;
    if (r < 1.00f) return UI_UNHEALTHY;
    return UI_BAD;
}

static bool g_seeded[4] = {false};

/* ── Push one value to chart + ring buffer + auto-scale ───────────── */
static void chart_push(lv_obj_t *c, lv_obj_t *l, int v, int thr, int def, int idx)
{
    if (l) lv_obj_set_style_text_color(l, sev_color(v, thr, def), 0);
    if (!c) return;
    lv_chart_series_t *sr = lv_chart_get_series_next(c, NULL);
    if (!sr) return;
    if (!g_seeded[idx]) {
        g_seeded[idx] = true;
        lv_chart_set_all_value(c, sr, v);
        for (int i = 0; i < CHART_PTS; i++) g_rings[idx][i] = v;
        g_ring_cnt[idx] = CHART_PTS;
    } else {
        g_rings[idx][g_ring_cnt[idx] % CHART_PTS] = v;
        g_ring_cnt[idx]++;
        lv_chart_set_next_value(c, sr, v);
    }
    autoscale(c, idx);
    lv_chart_refresh(c);
}

/* ── Percentage label ──────────────────────────────────────────────── */
static void update_pct_label(lv_obj_t *label, int val)
{
    lv_label_set_text_fmt(label, "%d%%", val);
}

/* ── Slider callbacks ──────────────────────────────────────────────── */
static void home_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    g_last_speed = (uint8_t)val;
    uint32_t now = lv_tick_get();
    if (now - g_slider_last_update_ms < 100) return;
    g_slider_last_update_ms = now;
    if (ui_home_fan_pct_label)
        update_pct_label(ui_home_fan_pct_label, val);
}

static void home_slider_released_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    g_last_speed = (uint8_t)val;
    if (ui_home_fan_pct_label)
        update_pct_label(ui_home_fan_pct_label, val);
    if (ui_home_fan_rpm_label)
        lv_label_set_text_fmt(ui_home_fan_rpm_label, "%u RPM", (unsigned)(val * 24));
    g_fan_on = (val > 0);
    if (ui_home_fan_btn_label)
        lv_label_set_text(ui_home_fan_btn_label,
            g_fan_on ? ui_lang_str(TXT_POWER_OFF) : ui_lang_str(TXT_POWER_ON));
    app_controller_set_fan_speed((uint8_t)val);
}

static void power_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (g_fan_on) {
        g_fan_on = false;
        lv_slider_set_value(ui_home_fan_slider, 0, LV_ANIM_ON);
        if (ui_home_fan_pct_label) lv_label_set_text(ui_home_fan_pct_label, "0%");
        if (ui_home_fan_rpm_label) lv_label_set_text(ui_home_fan_rpm_label, "0 RPM");
        if (ui_home_fan_btn_label) lv_label_set_text(ui_home_fan_btn_label, ui_lang_str(TXT_POWER_ON));
        app_controller_set_fan_speed(0);
    } else {
        if (g_last_speed == 0) g_last_speed = 50;
        g_fan_on = true;
        lv_slider_set_value(ui_home_fan_slider, g_last_speed, LV_ANIM_ON);
        if (ui_home_fan_pct_label)
            lv_label_set_text_fmt(ui_home_fan_pct_label, "%d%%", g_last_speed);
        if (ui_home_fan_rpm_label)
            lv_label_set_text_fmt(ui_home_fan_rpm_label, "%u RPM", (unsigned)(g_last_speed * 24));
        if (ui_home_fan_btn_label)
            lv_label_set_text(ui_home_fan_btn_label, ui_lang_str(TXT_POWER_OFF));
        app_controller_set_fan_speed(g_last_speed);
    }
    lv_obj_set_style_bg_color(btn,
        g_fan_on ? UI_ACCENT : UI_SURFACE_LIGHT, LV_PART_MAIN);
    if (ui_home_fan_btn_label)
        lv_obj_set_style_text_color(ui_home_fan_btn_label,
            g_fan_on ? lv_color_white() : UI_ACCENT, LV_PART_MAIN);
}

/* ── Create ─────────────────────────────────────────────────────────── */
lv_obj_t *ui_screen_home_create(lv_obj_t *parent)
{
    int pw = UI_CONTENT_W, ph = UI_CONTENT_H;
    int pad = UI_GUTTER, gap = UI_GUTTER;
    int left_w = (pw - pad * 2 - gap) / 2;
    int right_w = left_w;
    int card_h = ph - pad * 2;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, pw, ph);
    lv_obj_set_pos(root, UI_CONTENT_X, 0);
    lv_obj_add_style(root, lg_style_bg(), 0);
    lv_obj_set_style_bg_color(root, LG_BG_TOP, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_NONE);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* ── LEFT group ───────────────────────────────────────────────── */
    g_gas_card = lv_obj_create(root);
    lv_obj_set_size(g_gas_card, left_w, card_h);
    lv_obj_set_pos(g_gas_card, pad, pad);
    ui_flat_group(g_gas_card);

    /* Temp title */
    g_temp_title = lv_label_create(g_gas_card);
    lv_label_set_text(g_temp_title, ui_lang_str(TXT_TEMP));
    lv_obj_set_style_text_font(g_temp_title, ui_font_text(), 0);
    lv_obj_set_style_text_color(g_temp_title, UI_TEXT_MUTED, 0);
    lv_obj_set_pos(g_temp_title, 4, 8);

    /* Temp value — right side */
    ui_home_temp_label = lv_label_create(g_gas_card);
    lv_label_set_text(ui_home_temp_label, "--.- C");
    lv_obj_set_style_text_font(ui_home_temp_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_home_temp_label, UI_ACCENT, 0);
    lv_obj_align(ui_home_temp_label, LV_ALIGN_TOP_RIGHT, -4, 8);

    /* Temp chart */
    g_temp_chart = make_chart(g_gas_card, 4, 62, left_w - 8, 36, 0, 50);

    /* Holiday */
    g_holiday_label = lv_label_create(g_gas_card);
    lv_label_set_text(g_holiday_label, "");
    lv_obj_set_style_text_font(g_holiday_label, ui_font_text(), 0);
    lv_obj_set_style_text_color(g_holiday_label, lv_color_hex(0xFF6B6B), 0);
    lv_obj_add_flag(g_holiday_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(g_holiday_label, LV_ALIGN_TOP_RIGHT, -4, 8);

    g_gas_title = NULL;

    /* Three gas rows with LINE charts */
    const char *names[] = {"TVOC", "CO2", "CH2O"};
    const char *units[] = {"ug/m3", "ppm", "ug/m3"};
    int rows_top = 112, rh = 108, rg = 8;
    int rw = left_w;
    for (int i = 0; i < 3; i++) {
        int y = rows_top + i * (rh + rg);
        lv_obj_t *row = lv_obj_create(g_gas_card);
        lv_obj_set_size(row, rw, rh);
        lv_obj_set_pos(row, 0, y);
        ui_flat_group(row);
        g_gas_rows[i] = row;

        /* Name */
        lv_obj_t *nl = lv_label_create(row);
        lv_label_set_text(nl, names[i]);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nl, UI_TEXT_MUTED, 0);
        lv_obj_set_pos(nl, 4, 6);

        /* Value */
        lv_obj_t *vl = lv_label_create(row);
        lv_label_set_text(vl, "--");
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(vl, UI_GOOD, 0);
        lv_obj_align(vl, LV_ALIGN_TOP_RIGHT, -4, 8);
        if (i == 0) ui_home_tvoc_label = vl;
        if (i == 1) ui_home_co2_label = vl;
        if (i == 2) ui_home_ch2o_label = vl;

        /* Unit */
        lv_obj_t *ul = lv_label_create(row);
        lv_label_set_text(ul, units[i]);
        lv_obj_set_style_text_font(ul, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(ul, UI_TEXT_MUTED, 0);
        lv_obj_align_to(ul, vl, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

        /* Chart */
        lv_obj_t *ch = make_chart(row, 4, 56, rw - 8, 40, 0, 2000);
        if (i == 0) g_tvoc_chart = ch;
        if (i == 1) g_co2_chart = ch;
        if (i == 2) g_ch2o_chart = ch;
    }
    /* Y-axis auto-scales per-chart from live data — no hardcoded ranges */

    /* ── RIGHT: Fan control ────────────────────────────────────────── */
    g_fan_card = lv_obj_create(root);
    lv_obj_set_size(g_fan_card, right_w, card_h);
    lv_obj_set_pos(g_fan_card, pad + left_w + gap, pad);
    ui_flat_group(g_fan_card);

    g_fan_title = lv_label_create(g_fan_card);
    lv_label_set_text(g_fan_title, ui_lang_str(TXT_FAN_SPEED));
    lv_obj_set_style_text_font(g_fan_title, ui_font_text(), 0);
    lv_obj_set_style_text_color(g_fan_title, UI_TEXT_SECONDARY, 0);
    lv_obj_align(g_fan_title, LV_ALIGN_TOP_MID, 0, 14);

    ui_home_fan_pct_label = lv_label_create(g_fan_card);
    lv_label_set_text(ui_home_fan_pct_label, "0%");
    lv_obj_set_style_text_font(ui_home_fan_pct_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_home_fan_pct_label, UI_ACCENT, 0);
    lv_obj_align(ui_home_fan_pct_label, LV_ALIGN_CENTER, 0, -30);

    ui_home_fan_rpm_label = lv_label_create(g_fan_card);
    lv_label_set_text(ui_home_fan_rpm_label, "0 RPM");
    lv_obj_set_style_text_font(ui_home_fan_rpm_label, ui_font_text(), 0);
    lv_obj_set_style_text_color(ui_home_fan_rpm_label, UI_TEXT_MUTED, 0);
    lv_obj_align(ui_home_fan_rpm_label, LV_ALIGN_CENTER, 0, 14);

    /* Fan slider */
    int sw = right_w - 48;
    ui_home_fan_slider = lv_slider_create(g_fan_card);
    lv_obj_set_size(ui_home_fan_slider, sw, 10);
    lv_obj_align(ui_home_fan_slider, LV_ALIGN_CENTER, 0, 56);
    lv_obj_set_style_bg_color(ui_home_fan_slider, UI_SURFACE_LIGHT, 0);
    lv_obj_set_style_radius(ui_home_fan_slider, 5, 0);
    lv_obj_set_style_bg_color(ui_home_fan_slider, LG_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui_home_fan_slider, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_home_fan_slider, UI_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_radius(ui_home_fan_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_width(ui_home_fan_slider, 28, LV_PART_KNOB);
    lv_obj_set_style_height(ui_home_fan_slider, 28, LV_PART_KNOB);
    lv_slider_set_range(ui_home_fan_slider, 0, 100);
    lv_slider_set_value(ui_home_fan_slider, 0, LV_ANIM_OFF);
    lv_obj_add_flag(ui_home_fan_slider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(ui_home_fan_slider, 28);
    lv_obj_add_event_cb(ui_home_fan_slider, home_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_home_fan_slider, home_slider_released_cb, LV_EVENT_RELEASED, NULL);

    /* Power button */
    ui_home_fan_btn = lv_btn_create(g_fan_card);
    lv_obj_set_size(ui_home_fan_btn, right_w - 32, 48);
    lv_obj_align(ui_home_fan_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(ui_home_fan_btn, 14, 0);
    lv_obj_set_style_bg_color(ui_home_fan_btn, LG_ACCENT, 0);
    lv_obj_set_style_border_width(ui_home_fan_btn, 0, 0);
    ui_home_fan_btn_label = lv_label_create(ui_home_fan_btn);
    lv_label_set_text(ui_home_fan_btn_label, ui_lang_str(TXT_POWER_ON));
    lv_obj_set_style_text_font(ui_home_fan_btn_label, ui_font_text(), 0);
    lv_obj_set_style_text_color(ui_home_fan_btn_label, lv_color_white(), 0);
    lv_obj_center(ui_home_fan_btn_label);
    lv_obj_add_event_cb(ui_home_fan_btn, power_btn_cb, LV_EVENT_CLICKED, NULL);

    ESP_LOGI(TAG, "home screen created");
    return root;
}

/* ── Data updates ───────────────────────────────────────────────────── */
void ui_home_update_temp(float temp_c)
{
    if (ui_home_temp_label) {
        char b[16];
        snprintf(b, sizeof(b), "%.1f C", temp_c);
        lv_label_set_text(ui_home_temp_label, b);
    }
    if (g_temp_chart) {
        lv_chart_series_t *sr = lv_chart_get_series_next(g_temp_chart, NULL);
        if (sr) {
            int v = (int)(temp_c * 10);
            if (!g_seeded[0]) {
                g_seeded[0] = true;
                lv_chart_set_all_value(g_temp_chart, sr, v);
                for (int i = 0; i < CHART_PTS; i++) g_rings[0][i] = v;
                g_ring_cnt[0] = CHART_PTS;
            } else {
                g_rings[0][g_ring_cnt[0] % CHART_PTS] = v;
                g_ring_cnt[0]++;
                lv_chart_set_next_value(g_temp_chart, sr, v);
            }
            autoscale(g_temp_chart, 0);
            lv_chart_refresh(g_temp_chart);
        }
    }
}

void ui_home_update_tvoc(uint16_t val) {
    if (ui_home_tvoc_label) lv_label_set_text_fmt(ui_home_tvoc_label, "%u", val);
    uint16_t thr = 0;
    app_controller_get_alarm_thresholds(&thr, NULL, NULL);
    chart_push(g_tvoc_chart, ui_home_tvoc_label, (int)val, (int)thr, 1000, 1);
}
void ui_home_update_co2(uint16_t val) {
    if (ui_home_co2_label) lv_label_set_text_fmt(ui_home_co2_label, "%u", val);
    uint16_t thr = 0;
    app_controller_get_alarm_thresholds(NULL, &thr, NULL);
    chart_push(g_co2_chart, ui_home_co2_label, (int)val, (int)thr, 1000, 2);
}
void ui_home_update_ch2o(uint16_t val) {
    if (ui_home_ch2o_label) lv_label_set_text_fmt(ui_home_ch2o_label, "%u", val);
    uint16_t thr = 0;
    app_controller_get_alarm_thresholds(NULL, NULL, &thr);
    chart_push(g_ch2o_chart, ui_home_ch2o_label, (int)val, (int)thr, 100, 3);
}

void ui_home_update_fan_speed(uint8_t pct) {
    g_last_speed = pct;
    if (ui_home_fan_slider) lv_slider_set_value(ui_home_fan_slider, pct, LV_ANIM_OFF);
    if (ui_home_fan_pct_label) update_pct_label(ui_home_fan_pct_label, pct);
    g_fan_on = (pct > 0);
    if (ui_home_fan_btn_label)
        lv_label_set_text(ui_home_fan_btn_label, g_fan_on ? ui_lang_str(TXT_POWER_OFF) : ui_lang_str(TXT_POWER_ON));
    if (ui_home_fan_btn)
        lv_obj_set_style_bg_color(ui_home_fan_btn, g_fan_on ? UI_ACCENT : UI_SURFACE, 0);
    if (ui_home_fan_btn_label)
        lv_obj_set_style_text_color(ui_home_fan_btn_label, g_fan_on ? lv_color_white() : UI_ACCENT, 0);
}
void ui_home_update_fan_rpm(uint16_t rpm) {
    if (ui_home_fan_rpm_label) lv_label_set_text_fmt(ui_home_fan_rpm_label, "%u RPM", rpm);
}

void ui_home_update_holiday(void) {
    if (!g_holiday_label) return;
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    const char *name = holiday_get_name(ti.tm_mon + 1, ti.tm_mday);
    if (name && name[0]) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%s", name);
        lv_label_set_text(g_holiday_label, buf);
        lv_obj_remove_flag(g_holiday_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_holiday_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen_home_lang_update(void) {
    if (g_temp_title) lv_label_set_text(g_temp_title, ui_lang_str(TXT_TEMP));
    if (g_fan_title) lv_label_set_text(g_fan_title, ui_lang_str(TXT_FAN_SPEED));
    if (ui_home_fan_pct_label && ui_home_fan_slider)
        lv_label_set_text_fmt(ui_home_fan_pct_label, "%d%%", (int)lv_slider_get_value(ui_home_fan_slider));
    if (ui_home_fan_btn_label)
        lv_label_set_text(ui_home_fan_btn_label, g_fan_on ? ui_lang_str(TXT_POWER_OFF) : ui_lang_str(TXT_POWER_ON));
}

void ui_screen_home_theme_update(void) {
    if (g_temp_title) lv_obj_set_style_text_color(g_temp_title, UI_TEXT_MUTED, 0);
    if (ui_home_temp_label) lv_obj_set_style_text_color(ui_home_temp_label, UI_ACCENT, 0);
    if (g_fan_title) lv_obj_set_style_text_color(g_fan_title, UI_TEXT_SECONDARY, 0);
    for (int i = 0; i < 3; i++) {
        if (!g_gas_rows[i]) continue;
        uint32_t n = lv_obj_get_child_cnt(g_gas_rows[i]);
        for (uint32_t j = 0; j < n; j++) {
            lv_obj_t *ch = lv_obj_get_child(g_gas_rows[i], j);
            if (lv_obj_check_type(ch, &lv_label_class) &&
                lv_obj_get_style_text_font(ch, 0) == &lv_font_montserrat_14)
                lv_obj_set_style_text_color(ch, UI_TEXT_MUTED, 0);
        }
    }
    if (ui_home_fan_pct_label) lv_obj_set_style_text_color(ui_home_fan_pct_label, UI_TEXT, 0);
    if (ui_home_fan_rpm_label) lv_obj_set_style_text_color(ui_home_fan_rpm_label, UI_TEXT_MUTED, 0);
    if (ui_home_fan_slider) lv_obj_set_style_bg_color(ui_home_fan_slider, UI_SURFACE_LIGHT, 0);
    if (ui_home_fan_btn)
        lv_obj_set_style_bg_color(ui_home_fan_btn, g_fan_on ? UI_ACCENT : UI_SURFACE_LIGHT, 0);
    if (ui_home_fan_btn_label)
        lv_obj_set_style_text_color(ui_home_fan_btn_label, g_fan_on ? lv_color_white() : UI_ACCENT, 0);
}
