#include "ui_screen_settings.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_apple_anim.h"
#include "ui_manager.h"
#include "app_controller.h"
#include "board.h"
#include "lcd_tk043f1509.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include <time.h>
#define TAG "settings"

static lv_obj_t *g_lang_title = NULL;
static lv_obj_t *g_lang_cn_btn = NULL;
static lv_obj_t *g_lang_en_btn = NULL;
static lv_obj_t *g_theme_title = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_dark_btn = NULL;
static lv_obj_t *g_light_btn = NULL;
static lv_obj_t *g_auto_btn = NULL;
static lv_obj_t *g_bright_title = NULL;
static lv_obj_t *g_bright_slider = NULL;
static lv_obj_t *g_bright_pct_label = NULL;
static lv_obj_t *g_reset_lbl = NULL;

/* ── Brightness slider ────────────────────────────────────────────── */

static void bright_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    lv_obj_t *pct = (lv_obj_t *)lv_event_get_user_data(e);
    lv_label_set_text_fmt(pct, "%d%%", val);
    app_controller_set_brightness((uint8_t)val);
}

/* ── Language toggle ──────────────────────────────────────────────── */

static void update_lang_btn_styles(void)
{
    if (!g_lang_cn_btn || !g_lang_en_btn) return;
    ui_lang_t cur = ui_lang_get();
    lv_obj_set_style_bg_color(g_lang_cn_btn, cur == LANG_CN ? UI_ACCENT : UI_SURFACE_LIGHT, LV_PART_MAIN);
    lv_obj_t *cn_lbl = lv_obj_get_child(g_lang_cn_btn, 0);
    if (cn_lbl) lv_obj_set_style_text_color(cn_lbl, cur == LANG_CN ? lv_color_white() : UI_TEXT, LV_PART_MAIN);

    lv_obj_set_style_bg_color(g_lang_en_btn, cur == LANG_EN ? UI_ACCENT : UI_SURFACE_LIGHT, LV_PART_MAIN);
    lv_obj_t *en_lbl = lv_obj_get_child(g_lang_en_btn, 0);
    if (en_lbl) lv_obj_set_style_text_color(en_lbl, cur == LANG_EN ? lv_color_white() : UI_TEXT, LV_PART_MAIN);
}

static void lang_cn_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    if (ui_lang_get() != LANG_CN) {
        ui_lang_set(LANG_CN);
        update_lang_btn_styles();
    }
}

static void lang_en_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    if (ui_lang_get() != LANG_EN) {
        ui_lang_set(LANG_EN);
        update_lang_btn_styles();
    }
}

/* ── Theme select (direct, not cycle) ─────────────────────────────── */

static void update_theme_btn_styles(void)
{
    ui_theme_mode_t cur = ui_theme_get();
    lv_obj_t *btns[] = { g_dark_btn, g_light_btn, g_auto_btn };
    ui_theme_mode_t modes[] = { UI_THEME_DARK, UI_THEME_LIGHT, UI_THEME_AUTO };
    for (int i = 0; i < 3; i++) {
        if (!btns[i]) continue;
        bool sel = (cur == modes[i]);
        lv_obj_set_style_bg_color(btns[i], sel ? UI_ACCENT : UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_t *lbl = lv_obj_get_child(btns[i], 0);
        if (lbl)
            lv_obj_set_style_text_color(lbl, sel ? lv_color_white() : UI_TEXT, LV_PART_MAIN);
    }
}

static void theme_dark_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    ui_theme_set(UI_THEME_DARK);
    ui_manager_apply_theme();
    update_theme_btn_styles();
    update_lang_btn_styles();
}

static void theme_light_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    ui_theme_set(UI_THEME_LIGHT);
    ui_manager_apply_theme();
    update_theme_btn_styles();
    update_lang_btn_styles();
}

static void theme_auto_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "theme_auto_cb: start");
    apple_spring_bounce(lv_event_get_target(e));
    ui_theme_set(UI_THEME_AUTO);
    ESP_LOGI(TAG, "theme_auto_cb: after ui_theme_set");
    ui_manager_apply_theme();
    ESP_LOGI(TAG, "theme_auto_cb: after apply_theme");
    update_theme_btn_styles();
    update_lang_btn_styles();
    ESP_LOGI(TAG, "theme_auto_cb: done");
}

/* ── Time display (refreshed every 60s) ───────────────────────────── */

static void factory_reset_cb(lv_event_t *e)
{
    if (ui_manager_is_swipe_blocked()) return;

    static uint32_t last_tap = 0;
    static int tap_count = 0;
    uint32_t now = lv_tick_get();

    if (now - last_tap > 3000) tap_count = 0;
    last_tap = now;
    tap_count++;

    if (tap_count < 3) {
        apple_spring_bounce(lv_event_get_target(e));
        return;
    }

    tap_count = 0;
    nvs_flash_erase();
    esp_restart();
}

static void time_refresh_timer_cb(lv_timer_t *t)
{
    if (!g_time_label) return;

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year + 1900 < 2025) {
        lv_label_set_text(g_time_label, "--:--");
        return;
    }

    int sr_min = ui_theme_sunrise_min();
    int ss_min = ui_theme_sunset_min();
    int now_min = ti.tm_hour * 60 + ti.tm_min;

    /* Show the NEXT event: sunrise after sunset (or before sunrise),
     * sunset after sunrise (and before sunset). */
    bool is_day = (now_min >= sr_min && now_min < ss_min);
    const char *label = is_day ? ui_lang_str(TXT_SUNSET) : ui_lang_str(TXT_SUNRISE);
    int event_min = is_day ? ss_min : sr_min;

    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d  |  %s %02d:%02d",
             ti.tm_hour, ti.tm_min,
             label, event_min / 60, event_min % 60);
    lv_label_set_text(g_time_label, buf);
}

/* ── Create ────────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_settings_create(lv_obj_t *parent)
{
    int pw = UI_CONTENT_W, ph = UI_CONTENT_H;  /* 728 x 480 content area */

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

    int hw = (pw - 36) / 2, c1x = 12, c2x = 12 + hw + 12;  /* 346, 370 */
    int y1 = 12;

    /* ── Card 1 (col1): Language ──────────────────────────────────── */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, 150);
        lv_obj_set_pos(card, c1x, y1);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        g_lang_title = lv_label_create(card);
        lv_label_set_text(g_lang_title, ui_lang_str(TXT_LANGUAGE));
        lv_obj_set_style_text_font(g_lang_title, ui_font_text(), LV_PART_MAIN);
        lv_obj_add_style(g_lang_title, ui_style_title(), LV_PART_MAIN);
        lv_obj_set_pos(g_lang_title, 16, 18);

        int content_w = hw - 32;
        int btn_w = (content_w - 16) / 2;
        int btn_h = 46;
        int btn_y = 60;

        /* Chinese button */
        g_lang_cn_btn = lv_btn_create(card);
        lv_obj_set_size(g_lang_cn_btn, btn_w, btn_h);
        lv_obj_set_pos(g_lang_cn_btn, 0, btn_y);
        lv_obj_set_style_radius(g_lang_cn_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_lang_cn_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_lang_cn_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(g_lang_cn_btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_lang_cn_btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_lang_cn_btn, lang_cn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *cn_lbl = lv_label_create(g_lang_cn_btn);
        lv_label_set_text(cn_lbl, "中");
        lv_obj_set_style_text_font(cn_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(cn_lbl);

        /* English button */
        g_lang_en_btn = lv_btn_create(card);
        lv_obj_set_size(g_lang_en_btn, btn_w, btn_h);
        lv_obj_set_pos(g_lang_en_btn, btn_w + 16, btn_y);
        lv_obj_set_style_radius(g_lang_en_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_lang_en_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_lang_en_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(g_lang_en_btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_lang_en_btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_lang_en_btn, lang_en_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *en_lbl = lv_label_create(g_lang_en_btn);
        lv_label_set_text(en_lbl, "EN");
        lv_obj_set_style_text_font(en_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(en_lbl);

        update_lang_btn_styles();
    }

    /* ── Card 2 (col2, row1): Theme ───────────────────────────────── */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, 332);
        lv_obj_set_pos(card, c2x, y1);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        g_theme_title = lv_label_create(card);
        lv_label_set_text(g_theme_title, ui_lang_str(TXT_THEME));
        lv_obj_set_style_text_font(g_theme_title, ui_font_text(), LV_PART_MAIN);
        lv_obj_add_style(g_theme_title, ui_style_title(), LV_PART_MAIN);
        lv_obj_set_pos(g_theme_title, 16, 18);

        /* Time + sunrise/sunset label — centered, larger font */
        /* Centered vertically between title and theme buttons */
        g_time_label = lv_label_create(card);
        lv_label_set_text(g_time_label, "--:--");
        lv_obj_set_style_text_font(g_time_label, ui_font_cjk_28(), LV_PART_MAIN);
        lv_obj_set_style_text_color(g_time_label, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_align(g_time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(g_time_label, hw - 24);
        lv_obj_align(g_time_label, LV_ALIGN_CENTER, 0, 0);

        /* Three theme buttons in a flex row, centered */
        lv_obj_t *btn_row = lv_obj_create(card);
        lv_obj_set_size(btn_row, hw - 32, 46);
        lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -24);
        lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn_row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        int btn_w = (hw - 32 - 32) / 3;  /* 3 equal-width btns with gaps */
        int btn_h = 46;

        /* Dark button */
        g_dark_btn = lv_btn_create(btn_row);
        lv_obj_set_size(g_dark_btn, btn_w, btn_h);
        lv_obj_set_style_radius(g_dark_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_dark_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_dark_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(g_dark_btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_dark_btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_dark_btn, theme_dark_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *dl = lv_label_create(g_dark_btn);
        lv_label_set_text(dl, ui_lang_str(TXT_THEME_DARK));
        lv_obj_set_style_text_font(dl, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(dl);

        /* Light button */
        g_light_btn = lv_btn_create(btn_row);
        lv_obj_set_size(g_light_btn, btn_w, btn_h);
        lv_obj_set_style_radius(g_light_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_light_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_light_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(g_light_btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_light_btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_light_btn, theme_light_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *ll = lv_label_create(g_light_btn);
        lv_label_set_text(ll, ui_lang_str(TXT_THEME_LIGHT));
        lv_obj_set_style_text_font(ll, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(ll);

        /* Auto button */
        g_auto_btn = lv_btn_create(btn_row);
        lv_obj_set_size(g_auto_btn, btn_w, btn_h);
        lv_obj_set_style_radius(g_auto_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_auto_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_auto_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(g_auto_btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_auto_btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_auto_btn, theme_auto_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *al = lv_label_create(g_auto_btn);
        lv_label_set_text(al, ui_lang_str(TXT_THEME_AUTO));
        lv_obj_set_style_text_font(al, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(al);

        update_theme_btn_styles();

        /* Refresh time every 60s */
        lv_timer_create(time_refresh_timer_cb, 60000, NULL);
        time_refresh_timer_cb(NULL);
    }

    /* ── Card 3 (col1, row2): Brightness ──────────────────────────── */
    {
        int bright_y = y1 + 150 + 12;
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, 282);
        lv_obj_set_pos(card, c1x, bright_y);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        g_bright_title = lv_label_create(card);
        lv_label_set_text(g_bright_title, ui_lang_str(TXT_BRIGHTNESS));
        lv_obj_set_style_text_font(g_bright_title, ui_font_text(), LV_PART_MAIN);
        lv_obj_add_style(g_bright_title, ui_style_title(), LV_PART_MAIN);
        lv_obj_set_pos(g_bright_title, 16, 20);

        /* Percentage label — fixed width prevents text overlap on rapid change */
        lv_obj_t *pct_label = lv_label_create(card);
        g_bright_pct_label = pct_label;
        lv_label_set_text(pct_label, "80%");
        lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_36, LV_PART_MAIN);
        lv_obj_set_style_text_color(pct_label, UI_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_text_align(pct_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(pct_label, 130);
        lv_obj_align(pct_label, LV_ALIGN_CENTER, 0, -32);

        /* Slider — centered between pct label and markers */
        g_bright_slider = lv_slider_create(card);
        lv_obj_set_size(g_bright_slider, hw - 40, 10);
        lv_obj_align(g_bright_slider, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(g_bright_slider, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_radius(g_bright_slider, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_bright_slider, LG_ACCENT, LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_bright_slider, 3, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_bright_slider, UI_ACCENT, LV_PART_KNOB);
        lv_obj_set_style_radius(g_bright_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_width(g_bright_slider, 24, LV_PART_KNOB);
        lv_obj_set_style_height(g_bright_slider, 24, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(g_bright_slider, 6, LV_PART_KNOB);
        lv_obj_set_style_shadow_color(g_bright_slider, lv_color_black(), LV_PART_KNOB);
        lv_obj_set_style_shadow_opa(g_bright_slider, LV_OPA_30, LV_PART_KNOB);
        lv_slider_set_range(g_bright_slider, 0, 100);
        lv_obj_add_event_cb(g_bright_slider, bright_slider_cb, LV_EVENT_VALUE_CHANGED, pct_label);
        lv_obj_add_flag(g_bright_slider, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(g_bright_slider, 32);

        /* Markers: 0%/50%/100% — just below slider */
        {
            lv_obj_t *m0 = lv_label_create(card);
            lv_label_set_text(m0, "0%");
            lv_obj_set_style_text_font(m0, ui_font_text(), LV_PART_MAIN);
            lv_obj_set_style_text_color(m0, UI_TEXT_MUTED, LV_PART_MAIN);
            lv_obj_align_to(m0, g_bright_slider, LV_ALIGN_OUT_BOTTOM_LEFT, -2, 4);

            lv_obj_t *m50 = lv_label_create(card);
            lv_label_set_text(m50, "50%");
            lv_obj_set_style_text_font(m50, ui_font_text(), LV_PART_MAIN);
            lv_obj_set_style_text_color(m50, UI_TEXT_MUTED, LV_PART_MAIN);
            lv_obj_align_to(m50, g_bright_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

            lv_obj_t *m100 = lv_label_create(card);
            lv_label_set_text(m100, "100%");
            lv_obj_set_style_text_font(m100, ui_font_text(), LV_PART_MAIN);
            lv_obj_set_style_text_color(m100, UI_TEXT_MUTED, LV_PART_MAIN);
            lv_obj_align_to(m100, g_bright_slider, LV_ALIGN_OUT_BOTTOM_RIGHT, 2, 4);
        }

        /* Restore saved brightness */
        uint8_t saved = app_controller_get_brightness();
        lv_slider_set_value(g_bright_slider, saved, LV_ANIM_OFF);
        lv_label_set_text_fmt(pct_label, "%d%%", saved);
        lcd_set_backlight(saved);
    }

    /* ── Card 4 (col2, row2): Factory Reset ───────────────────────── */
    {
        int reset_y = y1 + 332 + 12;
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, 100);
        lv_obj_set_pos(card, c2x, reset_y);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);

        /* Large danger button — fills the card */
        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, hw - 32, 58);
        lv_obj_center(btn);
        lv_obj_set_style_radius(btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, UI_DANGER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, apple_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(btn, apple_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(btn, factory_reset_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *btn_lbl = lv_label_create(btn);
        g_reset_lbl = btn_lbl;
        lv_label_set_text(btn_lbl, ui_lang_str(TXT_RESET));
        lv_obj_set_style_text_color(btn_lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(btn_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_center(btn_lbl);
    }

    ESP_LOGI(TAG, "settings screen created");
    return root;
}

void ui_screen_settings_theme_update(void)
{
    update_lang_btn_styles();
    update_theme_btn_styles();
    if (g_bright_slider) {
        lv_obj_set_style_bg_color(g_bright_slider, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }
}

void ui_settings_update_brightness(uint8_t pct)
{
    if (!g_bright_slider) return;
    lvgl_port_lock(0);
    lv_slider_set_value(g_bright_slider, pct, LV_ANIM_OFF);
    if (g_bright_pct_label) lv_label_set_text_fmt(g_bright_pct_label, "%d%%", pct);
    lvgl_port_unlock();
}

void ui_screen_settings_lang_update(void)
{
    if (g_lang_title)
        lv_label_set_text(g_lang_title, ui_lang_str(TXT_LANGUAGE));
    update_lang_btn_styles();
    if (g_theme_title)
        lv_label_set_text(g_theme_title, ui_lang_str(TXT_THEME));
    if (g_bright_title)
        lv_label_set_text(g_bright_title, ui_lang_str(TXT_BRIGHTNESS));
    /* Refresh theme button labels */
    if (g_dark_btn) {
        lv_obj_t *l = lv_obj_get_child(g_dark_btn, 0);
        if (l) lv_label_set_text(l, ui_lang_str(TXT_THEME_DARK));
    }
    if (g_light_btn) {
        lv_obj_t *l = lv_obj_get_child(g_light_btn, 0);
        if (l) lv_label_set_text(l, ui_lang_str(TXT_THEME_LIGHT));
    }
    if (g_auto_btn) {
        lv_obj_t *l = lv_obj_get_child(g_auto_btn, 0);
        if (l) lv_label_set_text(l, ui_lang_str(TXT_THEME_AUTO));
    }

    if (g_reset_lbl)
        lv_label_set_text(g_reset_lbl, ui_lang_str(TXT_RESET));

    /* Update time label (sunset might be in different lang) */
    time_refresh_timer_cb(NULL);
}
