#include "ui_screen_brightness.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "board.h"
#include "lcd_tk043f1509.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "bright"

lv_obj_t *ui_bright_slider = NULL;
lv_obj_t *ui_bright_pct_label = NULL;

static lv_obj_t *g_dim_label = NULL;
static lv_obj_t *g_bright_label = NULL;

#define SUN_CHAR "*"
#define NVS_NS       "ui"
#define NVS_KEY_BRIGHT "brightness"

static void bright_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    lv_label_set_text_fmt(ui_bright_pct_label, "%d%%", val);
    lcd_set_backlight((uint8_t)val);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_BRIGHT, (uint8_t)val);
        nvs_commit(h);
        nvs_close(h);
    }
}

lv_obj_t *ui_screen_brightness_create(lv_obj_t *parent)
{
    int pw = LCD_WIDTH;
    int ph = LCD_HEIGHT;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, pw, ph);
    lv_obj_add_style(root, lg_style_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(root, LG_BG_TOP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);

    /* Main card — fill screen */
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_size(card, pw - 32, ph - 32);
    lv_obj_center(card);
    lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    /* Sun icon */
    lv_obj_t *sun = lv_label_create(card);
    lv_label_set_text(sun, SUN_CHAR);
    lv_obj_set_style_text_font(sun, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(sun, LG_ACCENT, LV_PART_MAIN);
    lv_obj_align(sun, LV_ALIGN_TOP_MID, 0, 40);

    /* Percentage */
    ui_bright_pct_label = lv_label_create(card);
    lv_label_set_text(ui_bright_pct_label, "80%");
    lv_obj_add_style(ui_bright_pct_label, lg_style_value_lg(), LV_PART_MAIN);
    lv_obj_align(ui_bright_pct_label, LV_ALIGN_CENTER, 0, -80);

    /* Slider — move down from center */
    int slider_w = pw - 120;
    ui_bright_slider = lv_slider_create(card);
    lv_obj_set_size(ui_bright_slider, slider_w, 10);
    lv_obj_set_pos(ui_bright_slider, ((pw - 32) - slider_w) / 2, (ph - 32) / 2 + 40);
    lv_obj_set_style_bg_color(ui_bright_slider, LG_BG_TOP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_bright_slider, LG_ACCENT, LV_PART_INDICATOR);
    /* Knob — sized to match other sliders */
    lv_obj_set_style_bg_color(ui_bright_slider, LG_ACCENT, LV_PART_KNOB);
    lv_obj_set_style_width(ui_bright_slider, 24, LV_PART_KNOB);
    lv_obj_set_style_height(ui_bright_slider, 24, LV_PART_KNOB);
    lv_obj_set_style_radius(ui_bright_slider, 14, LV_PART_KNOB);
    lv_obj_set_style_pad_ver(ui_bright_slider, 4, LV_PART_KNOB);
    lv_obj_set_style_pad_hor(ui_bright_slider, 4, LV_PART_KNOB);
    lv_slider_set_range(ui_bright_slider, 0, 100);
    lv_slider_set_value(ui_bright_slider, 80, LV_ANIM_OFF);  /* default, overridden below */
    lv_obj_add_event_cb(ui_bright_slider, bright_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(ui_bright_slider, LV_OBJ_FLAG_CLICKABLE);

    /* Restore saved brightness */
    uint8_t saved = 80;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t v;
        if (nvs_get_u8(h, NVS_KEY_BRIGHT, &v) == ESP_OK) saved = v;
        nvs_close(h);
    }
    lv_slider_set_value(ui_bright_slider, saved, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui_bright_pct_label, "%d%%", saved);
    lcd_set_backlight(saved);

    ESP_LOGI(TAG, "brightness screen created");
    return root;
}

void ui_screen_brightness_lang_update(void)
{
    if (g_dim_label)
        lv_label_set_text(g_dim_label, ui_lang_str(TXT_DIM));
    if (g_bright_label)
        lv_label_set_text(g_bright_label, ui_lang_str(TXT_BRIGHT));
}
