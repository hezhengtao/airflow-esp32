#include "ui_provision_prompt.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_manager.h"
#include "board.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "prov";
static lv_obj_t *g_prompt_overlay = NULL;

static lv_obj_t *g_title = NULL;
static lv_obj_t *g_msg = NULL;
static lv_obj_t *g_connect_lbl = NULL;
static lv_obj_t *g_skip_lbl = NULL;

static void dismiss_inner(void)
{
    if (!g_prompt_overlay) return;
    /* Hide instead of delete — avoids same LVGL event-list crash as boot overlay */
    lv_obj_add_flag(g_prompt_overlay, LV_OBJ_FLAG_HIDDEN);
    g_prompt_overlay = NULL;
    g_title = NULL;
    g_msg = NULL;
    g_connect_lbl = NULL;
    g_skip_lbl = NULL;
    ESP_LOGI(TAG, "provision prompt dismissed");
}

static void deferred_skip_cb(lv_timer_t *t)
{
    ESP_LOGI(TAG, "skip — showing first screen");
    dismiss_inner();
    ui_manager_show_first_screen();
}

static void deferred_connect_cb(lv_timer_t *t)
{
    ESP_LOGI(TAG, "connect — showing first screen + navigate to network");
    dismiss_inner();
    ui_manager_show_first_screen();
    ui_manager_navigate_to(UI_SCREEN_NETWORK);
}

/* Card click: left half = skip, right half = connect */
static void on_card_click(lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    /* Split at x=240 (screen center). pt.x < 240 → left/skip */
    if (pt.x < 240) {
        ESP_LOGI(TAG, "tapped LEFT side (x=%d) → skip", pt.x);
        lv_timer_t *t = lv_timer_create(deferred_skip_cb, 80, NULL);
        lv_timer_set_repeat_count(t, 1);
    } else {
        ESP_LOGI(TAG, "tapped RIGHT side (x=%d) → connect", pt.x);
        lv_timer_t *t = lv_timer_create(deferred_connect_cb, 80, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
}

lv_obj_t *ui_provision_prompt_create(lv_obj_t *parent)
{
    if (g_prompt_overlay) return g_prompt_overlay;

    int pw = LCD_WIDTH;
    int ph = LCD_HEIGHT;

    /* Full-screen black overlay */
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, pw, ph);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN);
    lv_obj_move_foreground(overlay);

    /* ── Card: centered, clickable, split left/right ──────────────── */
    int card_w = pw - 60;  /* 420 */
    int card_h = 340;
    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, card_w, card_h);
    lv_obj_center(card);
    lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    /* Make card clickable — whole card detects taps */
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, UI_SURFACE_LIGHT, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, on_card_click, LV_EVENT_CLICKED, NULL);

    /* Title */
    g_title = lv_label_create(card);
    lv_label_set_text(g_title, ui_lang_str(TXT_WELCOME));
    lv_obj_set_style_text_font(g_title, ui_font_text(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_title, UI_TEXT, LV_PART_MAIN);
    lv_obj_align(g_title, LV_ALIGN_TOP_MID, 0, 20);

    /* WiFi icon */
    lv_obj_t *wifi_icon = lv_label_create(card);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(wifi_icon, LG_ACCENT, LV_PART_MAIN);
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_MID, 0, 58);

    /* Instruction */
    g_msg = lv_label_create(card);
    lv_label_set_text(g_msg, ui_lang_str(TXT_PROV_MSG));
    lv_obj_set_style_text_font(g_msg, ui_font_text(), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_msg, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_align(g_msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_msg, LV_ALIGN_TOP_MID, 0, 130);

    /* ── Visual button hints (not clickable — card handles clicks) ── */
    /* Left: "跳过" label */
    g_skip_lbl = lv_label_create(card);
    lv_label_set_text(g_skip_lbl, ui_lang_str(TXT_SKIP));
    lv_obj_set_style_text_color(g_skip_lbl, LG_TEXT_TERTIARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_skip_lbl, ui_font_text(), LV_PART_MAIN);
    lv_obj_align(g_skip_lbl, LV_ALIGN_BOTTOM_MID, -105, -26);

    /* Right: "连接WiFi" label */
    g_connect_lbl = lv_label_create(card);
    lv_label_set_text(g_connect_lbl, ui_lang_str(TXT_CONNECT_WIFI));
    lv_obj_set_style_text_color(g_connect_lbl, LG_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_connect_lbl, ui_font_text(), LV_PART_MAIN);
    lv_obj_align(g_connect_lbl, LV_ALIGN_BOTTOM_MID, 105, -26);

    /* Divider line between left/right zones */
    lv_obj_t *div = lv_obj_create(card);
    lv_obj_set_size(div, 1, 40);
    lv_obj_align(div, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(div, UI_TEXT_MUTED, LV_PART_MAIN);
    lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(div, 0, LV_PART_MAIN);

    g_prompt_overlay = overlay;
    ESP_LOGI(TAG, "provision prompt created: tap LEFT=skip, RIGHT=connect");
    return overlay;
}

void ui_provision_prompt_lang_update(void)
{
    if (g_title)
        lv_label_set_text(g_title, ui_lang_str(TXT_WELCOME));
    if (g_msg)
        lv_label_set_text(g_msg, ui_lang_str(TXT_PROV_MSG));
    if (g_connect_lbl)
        lv_label_set_text(g_connect_lbl, ui_lang_str(TXT_CONNECT_WIFI));
    if (g_skip_lbl)
        lv_label_set_text(g_skip_lbl, ui_lang_str(TXT_SKIP));
}

void ui_provision_prompt_dismiss_inner(void)
{
    dismiss_inner();
}

void ui_provision_prompt_dismiss(void)
{
    /* The deferred_provision_cb timer in ui_manager.c polls g_net_connected
     * every 1.5s and handles dismiss + show-first-screen from the LVGL task
     * context.  This function exists as a safe no-lock fallback — it only
     * hides the prompt; the timer picks up the rest. */
    if (!g_prompt_overlay) return;
    if (!lvgl_port_lock(pdMS_TO_TICKS(500))) {
        return;  /* timer will handle it on next tick */
    }
    dismiss_inner();
    lvgl_port_unlock();
}
