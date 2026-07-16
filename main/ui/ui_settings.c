#include "ui_theme.h"
#include "ui_apple_anim.h"
#include "board.h"
#include "lvgl.h"

/* Exported alarm widgets for callbacks */
lv_obj_t *ui_settings_tvoc_slider = NULL;
lv_obj_t *ui_settings_co2_slider = NULL;
lv_obj_t *ui_settings_ch2o_slider = NULL;
lv_obj_t *ui_settings_auto_switch = NULL;

/* Apple press/release helper */
static void on_btn_press_release(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    if (code == LV_EVENT_PRESSED) {
        apple_press_effect(btn);
    } else if (code == LV_EVENT_RELEASED) {
        apple_release_effect(btn);
    }
}

lv_obj_t *ui_settings_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(root, UI_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    /* ── Header ────────────────────────────────────────────────── */
    lv_obj_t *header = lv_obj_create(root);
    lv_obj_set_size(header, LCD_WIDTH - 24, 44);
    lv_obj_set_pos(header, 12, 16);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);

    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 44, 44);
    lv_obj_add_style(back_btn, ui_style_btn_icon(), LV_PART_MAIN);
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_center(back_icon);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "设置");
    lv_obj_add_style(title, ui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    extern void ui_settings_on_back_click(lv_event_t *e);
    lv_obj_add_event_cb(back_btn, ui_settings_on_back_click, LV_EVENT_CLICKED, NULL);

    /* ── Scroll content ────────────────────────────────────────── */
    lv_obj_t *cont = lv_obj_create(root);
    lv_obj_set_size(cont, LCD_WIDTH - 24, LCD_HEIGHT - 76);
    lv_obj_set_pos(cont, 12, 68);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    #define SECTION_GAP { \
        lv_obj_t *gap = lv_obj_create(cont); \
        lv_obj_set_size(gap, LCD_WIDTH - 48, 8); \
        lv_obj_set_style_bg_opa(gap, LV_OPA_TRANSP, LV_PART_MAIN); \
        lv_obj_set_style_border_width(gap, 0, LV_PART_MAIN); \
    }

    /* ── Network section ─────────────────────────────────────── */
    lv_obj_t *net_section = lv_obj_create(cont);
    lv_obj_set_size(net_section, LCD_WIDTH - 48, 130);
    lv_obj_add_style(net_section, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *net_title = lv_label_create(net_section);
    lv_label_set_text(net_title, LV_SYMBOL_WIFI "  网络");
    lv_obj_add_style(net_title, ui_style_title(), LV_PART_MAIN);

    lv_obj_t *wifi_status_lbl = lv_label_create(net_section);
    lv_label_set_text(wifi_status_lbl, "未连接");
    lv_obj_set_style_text_color(wifi_status_lbl, UI_MODERATE, LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_status_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(wifi_status_lbl, LV_ALIGN_TOP_LEFT, 0, 36);

    lv_obj_t *wifi_config_btn = lv_btn_create(net_section);
    lv_obj_set_size(wifi_config_btn, 130, 36);
    lv_obj_align(wifi_config_btn, LV_ALIGN_BOTTOM_LEFT, 0, -4);
    lv_obj_add_style(wifi_config_btn, ui_style_btn_primary(), LV_PART_MAIN);
    lv_obj_t *wifi_btn_lbl = lv_label_create(wifi_config_btn);
    lv_label_set_text(wifi_btn_lbl, "配置 WiFi");
    lv_obj_center(wifi_btn_lbl);

    lv_obj_t *prov_btn = lv_btn_create(net_section);
    lv_obj_set_size(prov_btn, 100, 36);
    lv_obj_align(prov_btn, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
    lv_obj_add_style(prov_btn, ui_style_btn_ghost(), LV_PART_MAIN);
    lv_obj_t *prov_lbl = lv_label_create(prov_btn);
    lv_label_set_text(prov_lbl, "重置");
    lv_obj_center(prov_lbl);

    SECTION_GAP

    /* ── MQTT section ─────────────────────────────────────────── */
    lv_obj_t *mqtt_section = lv_obj_create(cont);
    lv_obj_set_size(mqtt_section, LCD_WIDTH - 48, 100);
    lv_obj_add_style(mqtt_section, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *mqtt_title = lv_label_create(mqtt_section);
    lv_label_set_text(mqtt_title, LV_SYMBOL_HOME "  MQTT 智能家居");
    lv_obj_add_style(mqtt_title, ui_style_title(), LV_PART_MAIN);

    lv_obj_t *mqtt_status_lbl = lv_label_create(mqtt_section);
    lv_label_set_text(mqtt_status_lbl, "MQTT 未连接");
    lv_obj_set_style_text_color(mqtt_status_lbl, UI_TEXT_MUTED, LV_PART_MAIN);
    lv_obj_set_style_text_font(mqtt_status_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(mqtt_status_lbl, LV_ALIGN_TOP_LEFT, 0, 36);

    SECTION_GAP

    /* ── Alarm thresholds ──────────────────────────────────────── */
    lv_obj_t *alarm_section = lv_obj_create(cont);
    lv_obj_set_size(alarm_section, LCD_WIDTH - 48, 250);
    lv_obj_add_style(alarm_section, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *alarm_title = lv_label_create(alarm_section);
    lv_label_set_text(alarm_title, LV_SYMBOL_BELL "  报警阈值");
    lv_obj_add_style(alarm_title, ui_style_title(), LV_PART_MAIN);

    /* Auto-fan toggle row */
    lv_obj_t *auto_row = lv_obj_create(alarm_section);
    lv_obj_set_size(auto_row, LCD_WIDTH - 72, 30);
    lv_obj_align(auto_row, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_set_style_bg_opa(auto_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(auto_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(auto_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *auto_label = lv_label_create(auto_row);
    lv_label_set_text(auto_label, "超标自动开启风扇");
    lv_obj_set_style_text_color(auto_label, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(auto_label, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *auto_switch = lv_switch_create(auto_row);
    lv_obj_set_style_bg_color(auto_switch, UI_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(auto_switch, UI_SURFACE_LIGHT, LV_PART_KNOB);

    /* TVOC slider */
    lv_obj_t *tvoc_row = lv_obj_create(alarm_section);
    lv_obj_set_size(tvoc_row, LCD_WIDTH - 72, 24);
    lv_obj_align(tvoc_row, LV_ALIGN_TOP_LEFT, 0, 74);
    lv_obj_set_style_bg_opa(tvoc_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tvoc_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(tvoc_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tvoc_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *tvoc_lbl = lv_label_create(tvoc_row);
    lv_label_set_text(tvoc_lbl, "TVOC");
    lv_obj_set_style_text_color(tvoc_lbl, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(tvoc_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *tvoc_val = lv_label_create(tvoc_row);
    lv_label_set_text(tvoc_val, "500");
    lv_obj_set_style_text_color(tvoc_val, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(tvoc_val, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *tvoc_slider = lv_slider_create(alarm_section);
    lv_obj_set_size(tvoc_slider, LCD_WIDTH - 76, 6);
    lv_obj_align(tvoc_slider, LV_ALIGN_TOP_LEFT, 0, 96);
    lv_slider_set_range(tvoc_slider, 100, 2000);
    lv_slider_set_value(tvoc_slider, 500, LV_ANIM_OFF);
    lv_obj_set_user_data(tvoc_slider, tvoc_val);

    /* CO2 slider */
    lv_obj_t *co2_row = lv_obj_create(alarm_section);
    lv_obj_set_size(co2_row, LCD_WIDTH - 72, 24);
    lv_obj_align(co2_row, LV_ALIGN_TOP_LEFT, 0, 128);
    lv_obj_set_style_bg_opa(co2_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(co2_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(co2_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(co2_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *co2_lbl = lv_label_create(co2_row);
    lv_label_set_text(co2_lbl, "CO2");
    lv_obj_set_style_text_color(co2_lbl, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(co2_lbl, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *co2_val = lv_label_create(co2_row);
    lv_label_set_text(co2_val, "1000");
    lv_obj_set_style_text_color(co2_val, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(co2_val, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *co2_slider = lv_slider_create(alarm_section);
    lv_obj_set_size(co2_slider, LCD_WIDTH - 76, 6);
    lv_obj_align(co2_slider, LV_ALIGN_TOP_LEFT, 0, 150);
    lv_slider_set_range(co2_slider, 400, 5000);
    lv_slider_set_value(co2_slider, 1000, LV_ANIM_OFF);
    lv_obj_set_user_data(co2_slider, co2_val);

    /* CH2O slider */
    lv_obj_t *ch2o_row = lv_obj_create(alarm_section);
    lv_obj_set_size(ch2o_row, LCD_WIDTH - 72, 24);
    lv_obj_align(ch2o_row, LV_ALIGN_TOP_LEFT, 0, 182);
    lv_obj_set_style_bg_opa(ch2o_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ch2o_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ch2o_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ch2o_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ch2o_lbl = lv_label_create(ch2o_row);
    lv_label_set_text(ch2o_lbl, "甲醛 CH2O");
    lv_obj_set_style_text_color(ch2o_lbl, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(ch2o_lbl, ui_font_text(), LV_PART_MAIN);

    lv_obj_t *ch2o_val = lv_label_create(ch2o_row);
    lv_label_set_text(ch2o_val, "100");
    lv_obj_set_style_text_color(ch2o_val, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(ch2o_val, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *ch2o_slider = lv_slider_create(alarm_section);
    lv_obj_set_size(ch2o_slider, LCD_WIDTH - 76, 6);
    lv_obj_align(ch2o_slider, LV_ALIGN_TOP_LEFT, 0, 204);
    lv_slider_set_range(ch2o_slider, 20, 500);
    lv_slider_set_value(ch2o_slider, 100, LV_ANIM_OFF);
    lv_obj_set_user_data(ch2o_slider, ch2o_val);

    /* Export for callbacks */
    ui_settings_tvoc_slider = tvoc_slider;
    ui_settings_co2_slider = co2_slider;
    ui_settings_ch2o_slider = ch2o_slider;
    ui_settings_auto_switch = auto_switch;

    SECTION_GAP

    /* ── Display ───────────────────────────────────────────────── */
    lv_obj_t *display_section = lv_obj_create(cont);
    lv_obj_set_size(display_section, LCD_WIDTH - 48, 90);
    lv_obj_add_style(display_section, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *disp_title = lv_label_create(display_section);
    lv_label_set_text(disp_title, LV_SYMBOL_IMAGE "  显示");
    lv_obj_add_style(disp_title, ui_style_title(), LV_PART_MAIN);

    lv_obj_t *bright_label = lv_label_create(display_section);
    lv_label_set_text(bright_label, "亮度");
    lv_obj_set_style_text_color(bright_label, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(bright_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(bright_label, LV_ALIGN_TOP_LEFT, 0, 36);

    lv_obj_t *bright_slider = lv_slider_create(display_section);
    lv_obj_set_size(bright_slider, LCD_WIDTH - 100, 6);
    lv_obj_align(bright_slider, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_slider_set_range(bright_slider, 10, 100);
    lv_slider_set_value(bright_slider, 80, LV_ANIM_OFF);

    SECTION_GAP

    /* ── System ────────────────────────────────────────────────── */
    lv_obj_t *sys_section = lv_obj_create(cont);
    lv_obj_set_size(sys_section, LCD_WIDTH - 48, 120);
    lv_obj_add_style(sys_section, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *sys_title = lv_label_create(sys_section);
    lv_label_set_text(sys_title, LV_SYMBOL_SETTINGS "  系统");
    lv_obj_add_style(sys_title, ui_style_title(), LV_PART_MAIN);

    /* Factory reset */
    lv_obj_t *reset_btn = lv_btn_create(sys_section);
    lv_obj_set_width(reset_btn, LCD_WIDTH - 72);
    lv_obj_set_height(reset_btn, 36);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(reset_btn, UI_DANGER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(reset_btn, 30, LV_PART_MAIN);
    lv_obj_set_style_radius(reset_btn, UI_RADIUS_BTN, LV_PART_MAIN);
    lv_obj_set_style_border_width(reset_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(reset_btn, UI_DANGER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(reset_btn, 0, LV_PART_MAIN);
    lv_obj_t *reset_lbl = lv_label_create(reset_btn);
    lv_label_set_text(reset_lbl, "恢复出厂设置");
    lv_obj_set_style_text_color(reset_lbl, UI_DANGER, LV_PART_MAIN);
    lv_obj_center(reset_lbl);

    /* ── Event callbacks ───────────────────────────────────────── */
    extern void ui_settings_on_wifi_click(lv_event_t *e);
    extern void ui_settings_on_reset_click(lv_event_t *e);
    extern void ui_settings_on_brightness_change(lv_event_t *e);
    extern void ui_settings_on_prov_reset(lv_event_t *e);
    extern void ui_settings_on_alarm_slider_change(lv_event_t *e);
    extern void ui_settings_on_auto_fan_change(lv_event_t *e);

    lv_obj_add_event_cb(wifi_config_btn, ui_settings_on_wifi_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(reset_btn, ui_settings_on_reset_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(bright_slider, ui_settings_on_brightness_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(prov_btn, ui_settings_on_prov_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tvoc_slider, ui_settings_on_alarm_slider_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(co2_slider, ui_settings_on_alarm_slider_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ch2o_slider, ui_settings_on_alarm_slider_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(auto_switch, ui_settings_on_auto_fan_change, LV_EVENT_VALUE_CHANGED, NULL);

    /* Apple press effects on all interactive buttons */
    lv_obj_add_event_cb(back_btn, on_btn_press_release, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(wifi_config_btn, on_btn_press_release, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(prov_btn, on_btn_press_release, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(reset_btn, on_btn_press_release, LV_EVENT_ALL, NULL);

    return root;
}
