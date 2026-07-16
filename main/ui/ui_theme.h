#pragma once

#include "lvgl.h"
#include "ui_design.h"

/* ── Theme mode ─────────────────────────────────────────────────── */
typedef enum {
    UI_THEME_DARK = 0,
    UI_THEME_LIGHT,
    UI_THEME_AUTO
} ui_theme_mode_t;

/* ── Theme API ──────────────────────────────────────────────────── */
void ui_theme_init(void);
void ui_theme_set(ui_theme_mode_t mode);
ui_theme_mode_t ui_theme_get(void);
bool ui_theme_is_dark(void);
const char *ui_theme_mode_label(void);

/* ── Color getters (used by UI_* / LG_* macros) ────────────────── */
lv_color_t ui_color_bg(void);
lv_color_t ui_color_surface(void);
lv_color_t ui_color_surface_light(void);
lv_color_t ui_color_text(void);
lv_color_t ui_color_text_secondary(void);
lv_color_t ui_color_text_muted(void);

/* ── Style getters ──────────────────────────────────────────────── */
lv_style_t *ui_style_glass(void);
lv_style_t *ui_style_glass_accent(void);
lv_style_t *ui_style_title(void);
lv_style_t *ui_style_value_lg(void);
lv_style_t *ui_style_value_md(void);
lv_style_t *ui_style_label(void);
lv_style_t *ui_style_caption(void);
lv_style_t *ui_style_btn_primary(void);
lv_style_t *ui_style_btn_ghost(void);
lv_style_t *ui_style_btn_icon(void);
lv_style_t *ui_style_slider(void);
lv_style_t *ui_style_pressed(void);
lv_style_t *ui_style_hairline(void);

lv_style_t *lg_style_card(void);
lv_style_t *lg_style_bg(void);
lv_style_t *lg_style_value_lg(void);
lv_style_t *lg_style_value_md(void);
lv_style_t *lg_style_caption(void);

/* ── Language update ─────────────────────────────────────────────── */
void ui_theme_lang_update(void);

/* ── Sun time helpers ────────────────────────────────────────────── */
int ui_theme_sunrise_min(void);
int ui_theme_sunset_min(void);
void ui_theme_try_sntp(void);   /* safe to call after WiFi connects */

/* ── AQI helpers ────────────────────────────────────────────────── */
lv_color_t ui_aqi_color(uint16_t tvoc_ugm3);
const char *ui_aqi_label(uint16_t tvoc_ugm3);
