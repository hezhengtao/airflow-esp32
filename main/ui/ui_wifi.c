#include "ui_theme.h"
#include "ui_apple_anim.h"
#include "board.h"
#include "lvgl.h"

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

lv_obj_t *ui_wifi_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(root, UI_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);

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

    extern void ui_wifi_on_back_click(lv_event_t *e);
    lv_obj_add_event_cb(back_btn, ui_wifi_on_back_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "WiFi 设置");
    lv_obj_add_style(title, ui_style_title(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    /* ── Content ────────────────────────────────────────────────── */
    lv_obj_t *cont = lv_obj_create(root);
    lv_obj_set_size(cont, LCD_WIDTH - 24, LCD_HEIGHT - 76);
    lv_obj_set_pos(cont, 12, 68);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ── Provisioning info card ────────────────────────────────── */
    lv_obj_t *info_card = lv_obj_create(cont);
    lv_obj_set_size(info_card, LCD_WIDTH - 48, 220);
    lv_obj_add_style(info_card, ui_style_glass_accent(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_card, 20, LV_PART_MAIN);

    lv_obj_t *info_icon = lv_label_create(info_card);
    lv_label_set_text(info_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(info_icon, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(info_icon, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_align(info_icon, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *info_title = lv_label_create(info_card);
    lv_label_set_text(info_title, "快速配网");
    lv_obj_add_style(info_title, ui_style_title(), LV_PART_MAIN);
    lv_obj_align(info_title, LV_ALIGN_TOP_MID, 0, 48);

    lv_obj_t *info_text = lv_label_create(info_card);
    lv_label_set_text(info_text,
        "1. 打开手机 WiFi 设置\n"
        "2. 连接到: AirPurifier-Setup\n"
        "3. 密码: setup1234\n"
        "4. 跟随配置页面引导\n"
        "5. 选择你家的 WiFi 网络");
    lv_obj_set_style_text_color(info_text, UI_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(info_text, 6, LV_PART_MAIN);
    lv_obj_align(info_text, LV_ALIGN_TOP_LEFT, 0, 82);

    /* ── Start provisioning button ─────────────────────────────── */
    lv_obj_t *start_btn = lv_btn_create(cont);
    lv_obj_set_size(start_btn, LCD_WIDTH - 48, 52);
    lv_obj_add_style(start_btn, ui_style_btn_primary(), LV_PART_MAIN);
    lv_obj_t *start_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_lbl, "开始配网");
    lv_obj_center(start_lbl);

    extern void ui_wifi_on_start_prov(lv_event_t *e);
    lv_obj_add_event_cb(start_btn, ui_wifi_on_start_prov, LV_EVENT_CLICKED, NULL);

    /* ── Manual SSID + Password ────────────────────────────────── */
    lv_obj_t *manual_card = lv_obj_create(cont);
    lv_obj_set_size(manual_card, LCD_WIDTH - 48, 180);
    lv_obj_add_style(manual_card, ui_style_glass(), LV_PART_MAIN);

    lv_obj_t *manual_title = lv_label_create(manual_card);
    lv_label_set_text(manual_title, "手动配置");
    lv_obj_add_style(manual_title, ui_style_title(), LV_PART_MAIN);

    lv_obj_t *ssid_ta = lv_textarea_create(manual_card);
    lv_obj_set_size(ssid_ta, LCD_WIDTH - 76, 42);
    lv_obj_align(ssid_ta, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_textarea_set_placeholder_text(ssid_ta, "WiFi 名称");
    lv_textarea_set_one_line(ssid_ta, true);
    lv_obj_set_style_bg_color(ssid_ta, UI_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(ssid_ta, UI_RADIUS_SM, LV_PART_MAIN);
    lv_obj_set_style_text_color(ssid_ta, UI_TEXT, LV_PART_MAIN);

    lv_obj_t *pass_ta = lv_textarea_create(manual_card);
    lv_obj_set_size(pass_ta, LCD_WIDTH - 76, 42);
    lv_obj_align(pass_ta, LV_ALIGN_TOP_LEFT, 0, 86);
    lv_textarea_set_placeholder_text(pass_ta, "WiFi 密码");
    lv_textarea_set_one_line(pass_ta, true);
    lv_textarea_set_password_mode(pass_ta, true);
    lv_obj_set_style_bg_color(pass_ta, UI_BG, LV_PART_MAIN);
    lv_obj_set_style_radius(pass_ta, UI_RADIUS_SM, LV_PART_MAIN);
    lv_obj_set_style_text_color(pass_ta, UI_TEXT, LV_PART_MAIN);

    lv_obj_t *connect_btn = lv_btn_create(manual_card);
    lv_obj_set_size(connect_btn, LCD_WIDTH - 76, 40);
    lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(connect_btn, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_radius(connect_btn, UI_RADIUS_BTN, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(connect_btn, 0, LV_PART_MAIN);
    lv_obj_t *conn_lbl = lv_label_create(connect_btn);
    lv_label_set_text(conn_lbl, "连接");
    lv_obj_center(conn_lbl);

    extern void ui_wifi_on_connect_click(lv_event_t *e);
    lv_obj_add_event_cb(connect_btn, ui_wifi_on_connect_click, LV_EVENT_CLICKED, NULL);

    /* Apple press effects on all buttons */
    lv_obj_add_event_cb(back_btn, on_btn_press_release, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(start_btn, on_btn_press_release, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(connect_btn, on_btn_press_release, LV_EVENT_ALL, NULL);

    return root;
}
