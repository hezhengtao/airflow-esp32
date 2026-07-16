#include "ui_boot.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "board.h"
#include "lcd_tk043f1509.h"
#include "esp_log.h"

static const char *TAG = "ui_boot";

static lv_obj_t *g_boot_overlay = NULL;
static void (*g_on_done)(void) = NULL;
static lv_timer_t *g_timer_light = NULL;
static lv_timer_t *g_timer_done = NULL;

/* Phase 2: display + backlight ON after LVGL has rendered the overlay */
static void boot_light_on_cb(lv_timer_t *t)
{
    g_timer_light = NULL;
    lcd_display_on();
    lcd_set_backlight(80);
    ESP_LOGI(TAG, "display + backlight ON");
}

/* Phase 3: call user callback, keep overlay as cover */
static void boot_done_cb(lv_timer_t *t)
{
    g_timer_done = NULL;
    if (g_on_done) {
        void (*cb)(void) = g_on_done;
        g_on_done = NULL;
        cb();
    }
}

void ui_boot_hide(void)
{
    if (g_timer_light) {
        lv_timer_delete(g_timer_light);
        g_timer_light = NULL;
    }
    if (g_timer_done) {
        lv_timer_delete(g_timer_done);
        g_timer_done = NULL;
    }

    if (g_boot_overlay) {
        lv_obj_delete(g_boot_overlay);
        g_boot_overlay = NULL;
        ESP_LOGI(TAG, "boot overlay deleted");
    }
}

void ui_boot_show(lv_obj_t *parent, void (*on_done)(void))
{
    g_on_done = on_done;

    /* Full-screen black overlay */
    g_boot_overlay = lv_obj_create(parent);
    lv_obj_set_size(g_boot_overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g_boot_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_boot_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_boot_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_boot_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_boot_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_boot_overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(g_boot_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Centered brand title only */
    lv_obj_t *title = lv_label_create(g_boot_overlay);
    lv_label_set_text(title, "AiRFLOW");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x4FC3F7), LV_PART_MAIN);
    lv_obj_center(title);

    /* Force LVGL to start rendering overlay ASAP */
    lv_obj_invalidate(g_boot_overlay);

    /* Phase 2 (300ms): LVGL has time to render → turn on display */
    g_timer_light = lv_timer_create(boot_light_on_cb, 300, NULL);
    lv_timer_set_repeat_count(g_timer_light, 1);

    /* Phase 3 (2800ms): remove overlay, proceed to main UI */
    g_timer_done = lv_timer_create(boot_done_cb, 2800, NULL);
    lv_timer_set_repeat_count(g_timer_done, 1);

    ESP_LOGI(TAG, "boot splash created (display off, rendering in background)");
}
