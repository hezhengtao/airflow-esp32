#include "ui_screen_network.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_manager.h"
#include "ui_apple_anim.h"
#include "board.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "wifi_prov.h"
#include "app_controller.h"
#include "libs/qrcode/lv_qrcode.h"
#include "settings.h"
#include "lwip/sockets.h"
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define TAG "net"

static lv_obj_t *g_kb = NULL;       /* text keyboard (SSID/pass/MQTT user/pass) */
static lv_obj_t *g_kb_num = NULL;   /* numeric keyboard (IP addresses) */
static lv_obj_t *g_kb_wrap = NULL;  /* wrapper for both keyboards */
static lv_obj_t *g_root = NULL;
static lv_obj_t *g_qr_fullscreen = NULL;
static lv_obj_t *g_wifi_list = NULL;
static lv_obj_t *g_qr_small = NULL;
static lv_obj_t *g_qr_url_label = NULL;
static bool g_scan_in_progress = false;
static lv_timer_t *g_scan_start_timer = NULL;
static int g_scan_fail_count = 0;

/* Boot-time scan cache — populated by a one-shot FreeRTOS task after WiFi connects,
 * so results are instantly available when the user opens the network screen. */
static wifi_ap_record_t g_ap_cache[12];
static int  g_ap_cache_count = 0;
static bool g_ap_cache_valid = false;

/* MQTT widgets */
static lv_obj_t *g_mqtt_uri_ta = NULL;
static lv_obj_t *g_mqtt_user_ta = NULL;
static lv_obj_t *g_mqtt_pass_ta = NULL;
static lv_obj_t *g_mqtt_status_label = NULL;
static lv_obj_t *g_connect_btn_lbl = NULL;
/* g_prov_btn_lbl removed -- prov button deleted */
static lv_obj_t *g_reset_btn_lbl = NULL;
static lv_obj_t *g_connect_btn = NULL;
static lv_obj_t *g_wifi_title = NULL;
static lv_obj_t *g_mqtt_title = NULL;

/* ── Keyboard helpers ─────────────────────────────────────────────── */

static int g_root_y = 0;

static lv_obj_t *g_active_ta = NULL;

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (!g_kb || !g_kb_num || !g_kb_wrap) return;

    /* Show cursor on the focused textarea */
    lv_obj_set_style_bg_color(ta, ui_color_text(), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_width(ta, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_width(ta, 0, LV_PART_CURSOR);
    g_active_ta = ta;

    /* MQTT URI → numeric keypad; everything else → text keyboard */
    bool is_ip = (ta == g_mqtt_uri_ta);

    if (is_ip) {
        lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_kb_num, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(g_kb_num, ta);
    } else {
        lv_obj_add_flag(g_kb_num, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(g_kb, ta);
    }

    lv_obj_remove_flag(g_kb_wrap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_kb_wrap);

    /* Auto-scroll root up if keyboard covers the focused field */
    lv_area_t ta_area, kb_area;
    lv_obj_get_coords(ta, &ta_area);
    lv_obj_get_coords(g_kb_wrap, &kb_area);
    if (ta_area.y2 > kb_area.y1) {
        g_root_y = kb_area.y1 - ta_area.y2 - 20;
        lv_obj_set_y(g_root, g_root_y);
    }
}

static void restore_root_y(void)
{
    if (g_root_y != 0) {
        lv_obj_set_y(g_root, 0);
        g_root_y = 0;
    }
}

static void hide_keyboard(void)
{
    if (g_kb_wrap) lv_obj_add_flag(g_kb_wrap, LV_OBJ_FLAG_HIDDEN);
    /* Hide cursor on previously focused textarea */
    if (g_active_ta) {
        lv_obj_set_style_bg_opa(g_active_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        g_active_ta = NULL;
    }
    if (g_kb) lv_keyboard_set_textarea(g_kb, NULL);
    if (g_kb_num) lv_keyboard_set_textarea(g_kb_num, NULL);
    restore_root_y();
}

static void kb_ok_cb(lv_event_t *e)
{
    hide_keyboard();
}

void ui_screen_network_hide_keyboard(void)
{
    hide_keyboard();
}

static void root_click_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) == g_root) {
        hide_keyboard();
        if (g_wifi_list) lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── QR code ─────────────────────────────────────────────────────── */

static void qr_dismiss_cb(lv_event_t *e)
{
    if (g_qr_fullscreen) {
        lv_obj_delete(g_qr_fullscreen);
        g_qr_fullscreen = NULL;
    }
}

static char *qr_get_url(void)
{
    static char url[128];

    /* Priority 1: STA has IP → show device dashboard URL */
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            snprintf(url, sizeof(url), "http://" IPSTR "/", IP2STR(&ip_info.ip));
            return url;
        }
    }

    /* Priority 2: AP mode → show captive portal URL */
    snprintf(url, sizeof(url), "http://192.168.4.1/");
    return url;
}

void qr_update_url(void)
{
    const char *url = qr_get_url();
    if (g_qr_small) lv_qrcode_update(g_qr_small, url, strlen(url));
    /* Keep the hint label as "扫码连接" — don't overwrite with the QR string */
}

static void qr_enlarge_cb(lv_event_t *e)
{
    int pw = LCD_WIDTH, ph = LCD_HEIGHT;

    g_qr_fullscreen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_qr_fullscreen, pw, ph);
    lv_obj_set_pos(g_qr_fullscreen, 0, 0);
    lv_obj_set_style_text_color(g_qr_fullscreen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_qr_fullscreen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_qr_fullscreen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_qr_fullscreen, 0, LV_PART_MAIN);

    lv_obj_t *qr = lv_qrcode_create(g_qr_fullscreen);
    lv_qrcode_set_size(qr, 280);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_data(qr, qr_get_url());
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *tap_lbl = lv_label_create(g_qr_fullscreen);
    lv_label_set_text(tap_lbl, ui_lang_str(TXT_TAP_TO_CLOSE));
    lv_obj_set_style_text_color(tap_lbl, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(tap_lbl, ui_font_text(), LV_PART_MAIN);
    lv_obj_align(tap_lbl, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_obj_add_flag(g_qr_fullscreen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_qr_fullscreen, qr_dismiss_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(g_qr_fullscreen);
}

/* ── WiFi scan list ─────────────────────────────────────────────── */

static void btn_press_cb(lv_event_t *e);
static void btn_release_cb(lv_event_t *e);

static void wifi_ssid_click_cb(lv_event_t *e)
{
    /* User data is on the row container; if event target is a child label,
     * walk up to find the row that holds the SSID pointer. */
    lv_obj_t *target = lv_event_get_target(e);
    const char *ssid = (const char *)lv_obj_get_user_data(target);
    while (!ssid && target) {
        target = lv_obj_get_parent(target);
        if (target) ssid = (const char *)lv_obj_get_user_data(target);
    }
    if (ssid && ui_net_ssid_ta) {
        lv_textarea_set_text(ui_net_ssid_ta, ssid);
    }
    if (g_wifi_list) lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_scan_timer_cb(lv_timer_t *t);
static void clean_ssid_list(lv_obj_t *list);

/* Signal color helpers */
#define SIG_WEAK    lv_color_make(0xFF, 0x57, 0x22)
#define SIG_FAIR    lv_color_make(0xFF, 0xC1, 0x07)
#define SIG_GOOD    lv_color_make(0x8B, 0xC3, 0x4A)
#define SIG_EXCEL   lv_color_make(0x4C, 0xAF, 0x50)

static void populate_ap_list(lv_obj_t *list, wifi_ap_record_t *aps, int ap_count)
{
    clean_ssid_list(list);

    /* Cap at 4 strongest APs so the list fits the input card without scroll.
     * esp_wifi returns records already sorted by RSSI (strongest first). */
    for (int i = 0; i < ap_count && i < 4; i++) {
        lv_color_t sig_color = SIG_WEAK;
        if (aps[i].rssi >= -50)      { sig_color = SIG_EXCEL; }
        else if (aps[i].rssi >= -60)  { sig_color = SIG_GOOD; }
        else if (aps[i].rssi >= -70)  { sig_color = SIG_FAIR; }

        bool secured = (aps[i].authmode != WIFI_AUTH_OPEN);

        const char *auth_str = "";
        switch (aps[i].authmode) {
        case WIFI_AUTH_WEP:           auth_str = "WEP";  break;
        case WIFI_AUTH_WPA_PSK:       auth_str = "WPA";  break;
        case WIFI_AUTH_WPA2_PSK:      auth_str = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK:  auth_str = "WPA2"; break;
        case WIFI_AUTH_WPA3_PSK:      auth_str = "WPA3"; break;
        case WIFI_AUTH_WPA2_WPA3_PSK: auth_str = "WPA3"; break;
        default: break;
        }

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), 46);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, (void *)strdup((const char *)aps[i].ssid));
        lv_obj_add_event_cb(row, wifi_ssid_click_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(row, btn_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(row, btn_release_cb, LV_EVENT_RELEASED, NULL);

        lv_obj_t *sig = lv_label_create(row);
        lv_label_set_text(sig, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(sig, sig_color, LV_PART_MAIN);
        lv_obj_set_style_text_font(sig, &lv_font_montserrat_24, LV_PART_MAIN);

        lv_obj_t *ssid_lbl = lv_label_create(row);
        lv_label_set_text(ssid_lbl, (const char *)aps[i].ssid);
        lv_obj_set_style_text_color(ssid_lbl, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(ssid_lbl, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_text_align(ssid_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_flex_grow(ssid_lbl, 1);

        if (secured && auth_str[0]) {
            lv_obj_t *auth_lbl = lv_label_create(row);
            lv_label_set_text(auth_lbl, auth_str);
            lv_obj_set_style_text_color(auth_lbl, UI_TEXT_MUTED, LV_PART_MAIN);
            lv_obj_set_style_text_font(auth_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        }
    }

    ESP_LOGI(TAG, "scan: %d APs", ap_count);
    for (int i = 0; i < ap_count && i < 8; i++) {
        ESP_LOGI(TAG, "  [%d] %-24s RSSI=%d ch=%d auth=%d",
                 i, (const char *)aps[i].ssid,
                 aps[i].rssi, aps[i].primary, aps[i].authmode);
    }
}

/* One-shot FreeRTOS task — blocks ~1-2s, runs after WiFi connects at boot.
 * Runs in its own task (not LVGL), so it doesn't freeze the UI. */
static void boot_scan_task(void *arg)
{
    wifi_scan_config_t scan_cfg = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 120, .max = 360 } },
    };

    esp_wifi_clear_ap_list();
    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Boot scan: start failed 0x%X", ret);
        vTaskDelete(NULL);
        return;
    }

    uint16_t ap_count = 12;
    esp_wifi_scan_get_ap_records(&ap_count, g_ap_cache);
    esp_wifi_clear_ap_list();
    g_ap_cache_count = (ap_count > 12) ? 12 : ap_count;
    g_ap_cache_valid = true;

    ESP_LOGI(TAG, "Boot scan cached %d APs", g_ap_cache_count);
    vTaskDelete(NULL);
}

void ui_screen_network_boot_scan(void)
{
    xTaskCreate(boot_scan_task, "bootscan", 4096, NULL, 2, NULL);
}

/* Non-blocking scan-done handler — registered once, populates visible list */
static void net_scan_done_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    uint16_t n = 0;
    if (!g_wifi_list || esp_wifi_scan_get_ap_num(&n) != ESP_OK || n == 0) { g_scan_in_progress = false; return; }
    wifi_ap_record_t *aps = calloc(n, sizeof(*aps));
    if (!aps) { g_scan_in_progress = false; return; }
    esp_wifi_scan_get_ap_records(&n, aps);
    int c = (n > 12) ? 12 : (int)n;
    memcpy(g_ap_cache, aps, c * sizeof(*aps));
    g_ap_cache_count = c; g_ap_cache_valid = true;
    if (!lv_obj_has_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN)) {
        clean_ssid_list(g_wifi_list);
        populate_ap_list(g_wifi_list, g_ap_cache, g_ap_cache_count);
    }
    ESP_LOGI(TAG, "scan done: %d APs", (int)n);
    free(aps); g_scan_in_progress = false;
}

static void do_wifi_scan(lv_obj_t *list)
{
    if (g_scan_in_progress) return;
    g_scan_in_progress = true;

    if (g_scan_start_timer) {
    /* Register SCAN_DONE handler once */
    { static bool registered = false;
      if (!registered) { registered = true;
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, net_scan_done_cb, NULL);
      }
    }
        lv_timer_delete(g_scan_start_timer);
        g_scan_start_timer = NULL;
    }

    /* Diagnostic: query WiFi state before scan */
    wifi_mode_t mode;
    esp_err_t mode_err = esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "scan diag: get_mode=%s(%d) mode=%d ini_ok=%d",
             esp_err_to_name(mode_err), mode_err, (int)mode,
             app_controller_is_wifi_ready());

    esp_wifi_clear_ap_list();

    /* Use blocking scan — same proven approach as wifi_prov.c */
    wifi_scan_config_t scan_cfg = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 120, .max = 360 } },
    };

    esp_err_t scan_err = esp_wifi_scan_start(&scan_cfg, false);
    if (scan_err != ESP_OK) {
        g_scan_in_progress = false;
        bool retry = (scan_err == ESP_ERR_WIFI_NOT_INIT
                      || scan_err == ESP_ERR_WIFI_NOT_STARTED
                      || scan_err == ESP_ERR_WIFI_STATE);

        if (retry && ++g_scan_fail_count <= 10) {
            ESP_LOGW(TAG, "WiFi not ready (0x%X), deferring scan (retry %d)",
                     scan_err, g_scan_fail_count);
            g_scan_start_timer = lv_timer_create(wifi_scan_timer_cb, 500, list);
            lv_timer_set_repeat_count(g_scan_start_timer, 1);
        } else {
            ESP_LOGE(TAG, "scan start failed: %s (0x%X), retries=%d",
                     esp_err_to_name(scan_err), scan_err, g_scan_fail_count);
            g_scan_fail_count = 0;
            clean_ssid_list(list);
            lv_obj_t *err_lbl = lv_label_create(list);
            char msg[32];
            snprintf(msg, sizeof(msg), "WiFi err:0x%X", (int)scan_err);
            lv_label_set_text(err_lbl, msg);
            lv_obj_set_style_text_color(err_lbl, UI_TEXT_MUTED, LV_PART_MAIN);
            lv_obj_set_style_text_font(err_lbl, ui_font_text(), LV_PART_MAIN);
            lv_obj_center(err_lbl);
        }
        return;
    }
    g_scan_fail_count = 0;
}

static void wifi_scan_timer_cb(lv_timer_t *t)
{
    g_scan_start_timer = NULL;
    do_wifi_scan((lv_obj_t *)lv_timer_get_user_data(t));
}

static void clean_ssid_list(lv_obj_t *list)
{
    uint32_t n = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(list, i);
        void *ud = lv_obj_get_user_data(child);
        if (ud) free(ud);
    }
    lv_obj_clean(list);
}

static void cancel_scan(void)
{
    g_scan_in_progress = false;
    if (g_scan_start_timer) {
        lv_timer_delete(g_scan_start_timer);
        g_scan_start_timer = NULL;
    }
}

static void ssid_deferred_cb(lv_timer_t *t)
{
    if (!g_wifi_list) return;

    hide_keyboard();
    restore_root_y();

    /* Toggle — hide if already visible */
    if (!lv_obj_has_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_wifi_list);

    /* Populate from boot cache first (instant, no blocking scan) */
    uint32_t n = lv_obj_get_child_cnt(g_wifi_list);
    if (n == 0 && g_ap_cache_valid && g_ap_cache_count > 0) {
        populate_ap_list(g_wifi_list, g_ap_cache, g_ap_cache_count);
    }

    /* If still empty (no cache), do a fresh scan */
    n = lv_obj_get_child_cnt(g_wifi_list);
    bool has_results = false;
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(g_wifi_list, i);
        if (lv_obj_get_user_data(child)) { has_results = true; break; }
    }
    if (!has_results && !g_scan_in_progress) {
        do_wifi_scan(g_wifi_list);
    }
}

static void ssid_show_list_cb(lv_event_t *e)
{
    /* Defer ALL LVGL operations — touching objects in CLICKED context crashes.
     * 40 ms delay avoids racing with touch-release and keyboard show/hide. */
    lv_timer_t *t = lv_timer_create(ssid_deferred_cb, 40, NULL);
    lv_timer_set_repeat_count(t, 1);
}

/* Refresh: cancel + clean + show + re-scan (does NOT close the list) */
static void refresh_deferred_cb(lv_timer_t *t)
{
    if (!g_wifi_list) return;
    cancel_scan();
    clean_ssid_list(g_wifi_list);
    lv_obj_remove_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_wifi_list);
    do_wifi_scan(g_wifi_list);
}

static void refresh_scan_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    lv_timer_t *t = lv_timer_create(refresh_deferred_cb, 1, NULL);
    lv_timer_set_repeat_count(t, 1);
}

/* ── WiFi status feedback (LVGL timer — no cross-task lock needed) ── */

static bool g_connect_pending = false;
static int g_connect_poll_count = 0;
static int g_connect_result_delay = 0;

static void do_ip_refresh(void);

static void connect_revert_cb(lv_timer_t *t)
{
    /* Phase 1: showing result ("连接成功"/"连接失败") — hold 2s then revert */
    if (g_connect_result_delay > 0) {
        g_connect_result_delay--;
        if (g_connect_result_delay <= 0) {
            if (g_connect_btn_lbl)
                lv_label_set_text(g_connect_btn_lbl, ui_lang_str(TXT_CONNECT));
            lv_timer_delete(t);
            g_connect_pending = false;
            g_connect_poll_count = 0;
        }
        return;
    }

    /* Phase 0: polling for connection — check WiFi STA state directly */
    g_connect_poll_count++;
    wifi_ap_record_t ap;
    bool connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);

    if (connected) {
        if (g_connect_btn_lbl)
            lv_label_set_text(g_connect_btn_lbl, ui_lang_str(TXT_WIFI_CONNECTED));
        g_connect_result_delay = 4;
        /* Refresh IP label with the new IP */
        do_ip_refresh();
    } else if (g_connect_poll_count >= 30) {
        if (g_connect_btn_lbl)
            lv_label_set_text(g_connect_btn_lbl, ui_lang_str(TXT_WIFI_CONNECT_FAILED));
        g_connect_result_delay = 4;
    }
}

/* ── Connect / Provision ────────────────────────────────────────── */

static void do_ip_refresh(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return;
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) return;
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    ui_update_ip(ip_str);
}

static void ip_refresh_cb(lv_event_t *e)
{
    do_ip_refresh();
}

static void conn_test_clear_cb(lv_timer_t *t)
{
    do_ip_refresh();
}

static void connectivity_test_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    if (!ui_net_ip_label) return;

    /* Non-blocking TCP connect with select() — never freezes UI */
    lv_label_set_text(ui_net_ip_label, ui_lang_str(TXT_IP_TESTING));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_aton("119.29.29.29", &addr.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        lv_label_set_text(ui_net_ip_label, ui_lang_str(TXT_IP_NO_INTERNET));
        goto restore;
    }

    /* Non-blocking mode */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);
        struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
        int ret = select(sock + 1, NULL, &wfds, NULL, &tv);

        bool ok = false;
        if (ret > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            ok = (err == 0);
        }
        lv_label_set_text(ui_net_ip_label, ok ? ui_lang_str(TXT_IP_CONNECTED)
                                               : ui_lang_str(TXT_IP_NO_INTERNET));
    }

    close(sock);

restore:
    lv_timer_t *t = lv_timer_create(conn_test_clear_cb, 3000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void connect_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    apple_spring_bounce(btn);
    hide_keyboard();
    restore_root_y();
    if (g_wifi_list) lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);

    const char *ssid = lv_textarea_get_text(ui_net_ssid_ta);
    const char *pass = lv_textarea_get_text(ui_net_pass_ta);

    /* Validate SSID not empty */
    if (!ssid || !ssid[0]) return;

    if (g_connect_btn_lbl)
        lv_label_set_text(g_connect_btn_lbl, ui_lang_str(TXT_WIFI_CONNECTING));

    g_connect_pending = true;
    g_connect_poll_count = 0;
    lv_timer_create(connect_revert_cb, 500, NULL);

    extern void app_controller_wifi_connect(const char *, const char *);
    app_controller_wifi_connect(ssid, pass);
}

static void reset_wifi_cb(lv_event_t *e)
{
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
    wifi_prov_erase_config();
    esp_restart();
}


static void btn_press_cb(lv_event_t *e)
{
    apple_press_effect(lv_event_get_target(e));
}

static void btn_release_cb(lv_event_t *e)
{
    apple_release_effect(lv_event_get_target(e));
}

/* ── MQTT helpers ───────────────────────────────────────────────── */

static void mqtt_normalize_uri(void)
{
    if (!g_mqtt_uri_ta) return;

    const char *raw = lv_textarea_get_text(g_mqtt_uri_ta);
    char buf[256];
    const char *host = raw;

    /* Strip mqtt:// or mqtts:// prefix if present */
    if (strncmp(raw, "mqtts://", 8) == 0) host = raw + 8;
    else if (strncmp(raw, "mqtt://", 7) == 0) host = raw + 7;

    /* Check if port is already specified */
    const char *colon = strchr(host, ':');
    const char *slash = strchr(host, '/');

    if (slash && slash < colon) colon = NULL;
    if (colon) {
        /* Already has port — just ensure prefix */
        if (raw == host) {
            snprintf(buf, sizeof(buf), "mqtt://%s", raw);
            lv_textarea_set_text(g_mqtt_uri_ta, buf);
        }
        return;
    }

    /* No port: append default 1883 */
    if (raw == host) {
        snprintf(buf, sizeof(buf), "mqtt://%s:1883", raw);
    } else {
        char prefix[16];
        int prefix_len = host - raw;
        memcpy(prefix, raw, prefix_len);
        prefix[prefix_len] = '\0';
        snprintf(buf, sizeof(buf), "%s%s:1883", prefix, host);
    }
    lv_textarea_set_text(g_mqtt_uri_ta, buf);
}

static void mqtt_clear_status_timer_cb(lv_timer_t *t)
{
    if (g_mqtt_status_label)
        lv_label_set_text(g_mqtt_status_label, "");
}

/* Non-blocking MQTT broker test — DNS+connect runs in a short-lived
 * FreeRTOS task so it never freezes the LVGL rendering loop.  The status
 * label updates when the task finishes (or the default 3s socket timeout). */
static bool g_mqtt_test_ok = false;
static void mqtt_test_done_timer_cb(lv_timer_t *t);

static void mqtt_test_task(void *arg)
{
    char *host = (char *)arg;
    char port_str[12];
    snprintf(port_str, sizeof(port_str), "%d", 1883);
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;

    g_mqtt_test_ok = false;
    if (getaddrinfo(host, port_str, &hints, &res) == 0 && res) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {0};
        memcpy(&addr, res->ai_addr,
               res->ai_addrlen < sizeof(addr) ? res->ai_addrlen : sizeof(addr));
        freeaddrinfo(res);
        if (sock >= 0) {
            struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            g_mqtt_test_ok = (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
            close(sock);
        }
    }
    free(host);
    lv_timer_create(mqtt_test_done_timer_cb, 1, NULL);
    vTaskDelete(NULL);
}

static void mqtt_test_done_timer_cb(lv_timer_t *t)
{
    if (g_mqtt_status_label)
        lv_label_set_text(g_mqtt_status_label,
            g_mqtt_test_ok ? ui_lang_str(TXT_MQTT_TEST_OK) : ui_lang_str(TXT_MQTT_TEST_FAIL));
    lv_timer_t *clr = lv_timer_create(mqtt_clear_status_timer_cb, 5000, NULL);
    lv_timer_set_repeat_count(clr, 1);
    lv_timer_delete(t);
}

static void mqtt_test_cb(lv_event_t *e)
{
    apple_spring_bounce(lv_event_get_target(e));
    mqtt_normalize_uri();
    if (!g_mqtt_status_label || !g_mqtt_uri_ta) return;

    const char *uri = lv_textarea_get_text(g_mqtt_uri_ta);
    if (!uri[0]) {
        lv_label_set_text(g_mqtt_status_label, ui_lang_str(TXT_MQTT_TEST_FAIL));
        return;
    }

    lv_label_set_text(g_mqtt_status_label, ui_lang_str(TXT_MQTT_TESTING));

    /* Parse mqtt://host:port → host string */
    const char *p = uri;
    if (strncmp(p, "mqtt://", 7) == 0) p += 7;
    const char *colon = strchr(p, ':');
    char *host = strdup(colon ? "" : p);
    if (colon) {
        size_t len = colon - p;
        host = malloc(len + 1);
        if (host) { memcpy(host, p, len); host[len] = 0; }
    }
    if (!host) return;

    xTaskCreate(mqtt_test_task, "mqttest", 3072, host, 1, NULL);
}

static void mqtt_save_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "mqtt_save: start");
    apple_spring_bounce(lv_event_get_target(e));
    mqtt_normalize_uri();
    if (!g_mqtt_status_label) return;

    if (g_mqtt_uri_ta) {
        const char *s = lv_textarea_get_text(g_mqtt_uri_ta);
        settings_save_str(NVS_KEY_MQTT_URI, s);
    }
    if (g_mqtt_user_ta) {
        const char *s = lv_textarea_get_text(g_mqtt_user_ta);
        settings_save_str(NVS_KEY_MQTT_USER, s);
    }
    if (g_mqtt_pass_ta) {
        const char *s = lv_textarea_get_text(g_mqtt_pass_ta);
        settings_save_str(NVS_KEY_MQTT_PASS, s);
    }
    ESP_LOGI(TAG, "mqtt_save: committing NVS...");
    settings_commit();
    ESP_LOGI(TAG, "mqtt_save: commit done");

    /* Reload in-memory settings + reconnect MQTT with new broker */
    app_controller_mqtt_apply();

    lv_label_set_text(g_mqtt_status_label, ui_lang_str(TXT_MQTT_SAVED));

    lv_timer_t *t = lv_timer_create(mqtt_clear_status_timer_cb, 5000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void read_mqtt_settings(void)
{
    char buf[128];
    if (settings_get_str(NVS_KEY_MQTT_URI, buf, sizeof(buf)) == ESP_OK && g_mqtt_uri_ta)
        lv_textarea_set_text(g_mqtt_uri_ta, buf);
    if (settings_get_str(NVS_KEY_MQTT_USER, buf, sizeof(buf)) == ESP_OK && g_mqtt_user_ta)
        lv_textarea_set_text(g_mqtt_user_ta, buf);
    if (settings_get_str(NVS_KEY_MQTT_PASS, buf, sizeof(buf)) == ESP_OK && g_mqtt_pass_ta)
        lv_textarea_set_text(g_mqtt_pass_ta, buf);
}

/* ── Public handles ─────────────────────────────────────────────── */

lv_obj_t *ui_net_wifi_label = NULL;
lv_obj_t *ui_net_ip_label = NULL;
lv_obj_t *ui_net_mqtt_label = NULL;
lv_obj_t *ui_net_ssid_ta = NULL;
lv_obj_t *ui_net_pass_ta = NULL;
lv_obj_t *ui_net_theme_btn = NULL;

/* ── Create ─────────────────────────────────────────────────────── */

lv_obj_t *ui_screen_network_create(lv_obj_t *parent)
{
    int pw = UI_CONTENT_W, ph = UI_CONTENT_H;  /* 728 x 480 content area */

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, pw, ph);
    lv_obj_set_pos(root, UI_CONTENT_X, 0);
    lv_obj_add_style(root, lg_style_bg(), LV_PART_MAIN);
    lv_obj_set_style_text_color(root, LG_BG_TOP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(root, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(root, root_click_cb, LV_EVENT_CLICKED, NULL);
    g_root = root;

    int card_w = pw - 24;  /* full: 704 */
    int hw = (pw - 36) / 2, col2x = 12 + hw + 12;  /* 346 + 370 */

    /* ═══ 1. Status row: info (left) + enlarged QR (right) ═══════════ */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, card_w, 158);
        lv_obj_set_pos(card, 12, 12);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        /* WiFi 状态 */
        ui_net_wifi_label = lv_label_create(card);
        lv_label_set_text(ui_net_wifi_label, "WiFi: --");
        lv_obj_set_style_text_color(ui_net_wifi_label, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_net_wifi_label, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(ui_net_wifi_label, 16, 16);

        /* IP */
        ui_net_ip_label = lv_label_create(card);
        lv_label_set_text(ui_net_ip_label, "IP: --");
        lv_obj_set_style_text_color(ui_net_ip_label, UI_TEXT_MUTED, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_net_ip_label, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(ui_net_ip_label, 16, 54);

        /* MQTT 状态 */
        ui_net_mqtt_label = lv_label_create(card);
        lv_label_set_text(ui_net_mqtt_label, "MQTT: --");
        lv_obj_set_style_text_color(ui_net_mqtt_label, UI_TEXT_MUTED, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_net_mqtt_label, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(ui_net_mqtt_label, 16, 92);

        /* 连通性测试 + IP刷新按钮 (信息区下方) */
        lv_obj_t *conn_test_btn = lv_btn_create(card);
        lv_obj_set_size(conn_test_btn, 40, 40);
        lv_obj_set_pos(conn_test_btn, 16, 130);
        lv_obj_set_style_radius(conn_test_btn, UI_RADIUS_SM, LV_PART_MAIN);
        lv_obj_set_style_bg_color(conn_test_btn, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_border_width(conn_test_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(conn_test_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(conn_test_btn, connectivity_test_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *conn_test_icon = lv_label_create(conn_test_btn);
        lv_label_set_text(conn_test_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(conn_test_icon, lv_color_hex(0x66BB6A), LV_PART_MAIN);
        lv_obj_set_style_text_font(conn_test_icon, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(conn_test_icon);

        lv_obj_t *ip_refresh_btn = lv_btn_create(card);
        lv_obj_set_size(ip_refresh_btn, 40, 40);
        lv_obj_set_pos(ip_refresh_btn, 64, 130);
        lv_obj_set_style_radius(ip_refresh_btn, UI_RADIUS_SM, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ip_refresh_btn, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_border_width(ip_refresh_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ip_refresh_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(ip_refresh_btn, ip_refresh_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *ip_refresh_icon = lv_label_create(ip_refresh_btn);
        lv_label_set_text(ip_refresh_icon, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(ip_refresh_icon, UI_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_text_font(ip_refresh_icon, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_center(ip_refresh_icon);

        /* QR 码 (右侧，放大到 138) + 下方提示文字 */
        int qr_sz = 138;
        g_qr_small = lv_qrcode_create(card);
        lv_qrcode_set_size(g_qr_small, qr_sz);
        lv_qrcode_set_dark_color(g_qr_small, lv_color_black());
        lv_qrcode_set_light_color(g_qr_small, lv_color_white());
        lv_qrcode_set_data(g_qr_small, qr_get_url());
        lv_obj_set_pos(g_qr_small, card_w - qr_sz - 150, 10);
        lv_obj_add_flag(g_qr_small, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_qr_small, qr_enlarge_cb, LV_EVENT_CLICKED, NULL);

        g_qr_url_label = lv_label_create(card);
        lv_label_set_text(g_qr_url_label, ui_lang_str(TXT_SCAN_QR));
        lv_obj_set_style_text_color(g_qr_url_label, UI_TEXT_MUTED, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_qr_url_label, ui_font_text(), LV_PART_MAIN);
        lv_obj_align_to(g_qr_url_label, g_qr_small, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    }

    /* ═══ 2. WiFi inputs (left column) ════════════════════════════ */
    int col_y = 180;
    int col_h = 200;   /* tightened: buttons at col_y+col_h+10 = 390, fit safely */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, col_h);
        lv_obj_set_pos(card, 12, col_y);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        /* 标题 */
        lv_obj_t *title = lv_label_create(card);
        g_wifi_title = title;
        lv_label_set_text(title, ui_lang_str(TXT_WIFI_SETUP));
        lv_obj_set_style_text_color(title, UI_TEXT_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(title, 16, 14);

        int ta_w = hw - 32;  /* 408 */
        int ta_h = 48;

        /* SSID 输入框 (留出右侧扫描按钮空间) */
        ui_net_ssid_ta = lv_textarea_create(card);
        lv_obj_set_size(ui_net_ssid_ta, ta_w - 52, ta_h);
        lv_obj_set_pos(ui_net_ssid_ta, 16, 56);
        lv_textarea_set_placeholder_text(ui_net_ssid_ta, ui_lang_str(TXT_WIFI_NAME));
        lv_textarea_set_one_line(ui_net_ssid_ta, true);
        lv_obj_set_style_bg_color(ui_net_ssid_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui_net_ssid_ta, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(ui_net_ssid_ta, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui_net_ssid_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui_net_ssid_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_net_ssid_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_net_ssid_ta, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_width(ui_net_ssid_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(ui_net_ssid_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ui_net_ssid_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_add_event_cb(ui_net_ssid_ta, ssid_show_list_cb, LV_EVENT_CLICKED, NULL);

        /* 手动扫描按钮 (SSID右侧) */
        lv_obj_t *scan_btn = lv_btn_create(card);
        lv_obj_set_size(scan_btn, 44, 44);
        lv_obj_set_pos(scan_btn, 16 + ta_w - 48, 58);
        lv_obj_set_style_radius(scan_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_bg_color(scan_btn, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_border_width(scan_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(scan_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(scan_btn, btn_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(scan_btn, btn_release_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(scan_btn, refresh_scan_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *scan_icon = lv_label_create(scan_btn);
        lv_label_set_text(scan_icon, LV_SYMBOL_REFRESH);
        lv_obj_set_style_bg_color(scan_icon, UI_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_text_font(scan_icon, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_center(scan_icon);

        /* 密码输入框 (留出右侧眼睛按钮空间) */
        ui_net_pass_ta = lv_textarea_create(card);
        lv_obj_set_size(ui_net_pass_ta, ta_w - 52, ta_h);
        lv_obj_set_pos(ui_net_pass_ta, 16, 118);
        lv_textarea_set_placeholder_text(ui_net_pass_ta, ui_lang_str(TXT_WIFI_PASS));
        lv_textarea_set_one_line(ui_net_pass_ta, true);
        lv_textarea_set_password_mode(ui_net_pass_ta, true);
        lv_obj_set_style_bg_color(ui_net_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui_net_pass_ta, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(ui_net_pass_ta, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui_net_pass_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui_net_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_net_pass_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(ui_net_pass_ta, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_width(ui_net_pass_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(ui_net_pass_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ui_net_pass_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_add_event_cb(ui_net_pass_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);

        /* 密码可见性切换按钮 (眼睛图标) */
        lv_obj_t *eye_btn = lv_btn_create(card);
        lv_obj_set_size(eye_btn, 44, 44);
        lv_obj_set_pos(eye_btn, 16 + ta_w - 48, 120);
        lv_obj_set_style_radius(eye_btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_bg_color(eye_btn, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_border_width(eye_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(eye_btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(eye_btn, btn_press_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(eye_btn, btn_release_cb, LV_EVENT_RELEASED, NULL);

        lv_obj_t *eye_icon = lv_label_create(eye_btn);
        lv_label_set_text(eye_icon, LV_SYMBOL_EYE_CLOSE);
        lv_obj_set_style_text_color(eye_icon, UI_TEXT_MUTED, LV_PART_MAIN);
        lv_obj_set_style_text_font(eye_icon, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_center(eye_icon);

        /* Toggle password visibility on click */
        void eye_toggle_cb(lv_event_t *e)
        {
            lv_obj_t *btn = lv_event_get_target(e);
            lv_obj_t *icon = lv_obj_get_child(btn, 0);
            bool hidden = lv_textarea_get_password_mode(ui_net_pass_ta);
            lv_textarea_set_password_mode(ui_net_pass_ta, !hidden);
            lv_label_set_text(icon, hidden ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
            lv_obj_set_style_text_color(icon, hidden ? UI_ACCENT : UI_TEXT_MUTED, LV_PART_MAIN);
        }
        lv_obj_add_event_cb(eye_btn, eye_toggle_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ═══════════════════════════════════════════════════════════════
     * 3. MQTT 配置卡片 (y=452, h=338)
     * ═══════════════════════════════════════════════════════════════ */
    {
        lv_obj_t *card = lv_obj_create(root);
        lv_obj_set_size(card, hw, col_h);
        lv_obj_set_pos(card, col2x, col_y);
        lv_obj_add_style(card, lg_style_card(), LV_PART_MAIN);
        ui_flat_surface(card);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);

        /* 标题 */
        lv_obj_t *title = lv_label_create(card);
        g_mqtt_title = title;
        lv_label_set_text(title, ui_lang_str(TXT_MQTT_CONFIG));
        lv_obj_set_style_text_color(title, UI_TEXT_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(title, 16, 14);

        int ta_w = hw - 32;
        int ta_h = 48;

        /* URI 输入框 */
        g_mqtt_uri_ta = lv_textarea_create(card);
        lv_obj_set_size(g_mqtt_uri_ta, ta_w, ta_h);
        lv_obj_set_pos(g_mqtt_uri_ta, 16, 60);
        lv_textarea_set_placeholder_text(g_mqtt_uri_ta, "mqtt://192.168.1.1:1883");
        lv_textarea_set_one_line(g_mqtt_uri_ta, true);
        lv_obj_set_style_bg_color(g_mqtt_uri_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_mqtt_uri_ta, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(g_mqtt_uri_ta, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_mqtt_uri_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_mqtt_uri_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_uri_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_mqtt_uri_ta, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_width(g_mqtt_uri_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_uri_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_uri_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_add_event_cb(g_mqtt_uri_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);

        int half_w = (ta_w - 8) / 2;

        /* 用户名 (左侧) */
        g_mqtt_user_ta = lv_textarea_create(card);
        lv_obj_set_size(g_mqtt_user_ta, half_w, ta_h);
        lv_obj_set_pos(g_mqtt_user_ta, 16, 120);
        lv_textarea_set_placeholder_text(g_mqtt_user_ta, ui_lang_str(TXT_MQTT_USER));
        lv_textarea_set_one_line(g_mqtt_user_ta, true);
        lv_obj_set_style_bg_color(g_mqtt_user_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_mqtt_user_ta, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(g_mqtt_user_ta, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_mqtt_user_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_mqtt_user_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_user_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_mqtt_user_ta, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_width(g_mqtt_user_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_user_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_user_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_add_event_cb(g_mqtt_user_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);

        /* 密码 (右侧) */
        g_mqtt_pass_ta = lv_textarea_create(card);
        lv_obj_set_size(g_mqtt_pass_ta, half_w, ta_h);
        lv_obj_set_pos(g_mqtt_pass_ta, 16 + half_w + 8, 120);
        lv_textarea_set_placeholder_text(g_mqtt_pass_ta, ui_lang_str(TXT_MQTT_PASS));
        lv_textarea_set_one_line(g_mqtt_pass_ta, true);
        lv_textarea_set_password_mode(g_mqtt_pass_ta, true);
        lv_obj_set_style_bg_color(g_mqtt_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_mqtt_pass_ta, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(g_mqtt_pass_ta, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_border_width(g_mqtt_pass_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_mqtt_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_pass_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_mqtt_pass_ta, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_style_width(g_mqtt_pass_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_pass_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_pass_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_add_event_cb(g_mqtt_pass_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);

        /* 状态标签 — below the user/pass fields (buttons moved to shared row) */
        g_mqtt_status_label = lv_label_create(card);
        lv_label_set_text(g_mqtt_status_label, "");
        lv_obj_set_style_text_color(g_mqtt_status_label, UI_TEXT_SECONDARY, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_mqtt_status_label, ui_font_text(), LV_PART_MAIN);
        lv_obj_set_pos(g_mqtt_status_label, 16, 176);
    }

    /* ═══ 2b. Shared action row: 连接│重置│测试│保存 ═══════════════ */
    {
        int row_y = col_y + col_h + 10;   /* 180+200+10 = 390 */
        int n = 4, gap = 12;
        int bw = (card_w - gap * (n - 1)) / n;   /* (704-36)/4 = 167 */
        int bh = 50;
        int bx = 12;

        struct { lv_event_cb_t cb; const char *txt; lv_color_t bg; bool ghost; lv_obj_t **lbl_out; lv_obj_t **btn_out; } B[] = {
            { connect_cb,    ui_lang_str(TXT_CONNECT),    UI_ACCENT,               false, &g_connect_btn_lbl, &g_connect_btn },
            { reset_wifi_cb, ui_lang_str(TXT_RESET_WIFI), lv_color_hex(0xE57373),  false, &g_reset_btn_lbl,   NULL },
            { mqtt_test_cb,  ui_lang_str(TXT_MQTT_TEST),  UI_ACCENT,               false, NULL,               NULL },
            { mqtt_save_cb,  ui_lang_str(TXT_MQTT_SAVE),  UI_ACCENT,               false, NULL,               NULL },
        };
        for (int i = 0; i < n; i++) {
            lv_obj_t *btn = lv_btn_create(root);
            lv_obj_set_size(btn, bw, bh);
            lv_obj_set_pos(btn, bx + i * (bw + gap), row_y);
            lv_obj_set_style_radius(btn, UI_RADIUS_BTN, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, B[i].bg, LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
            lv_obj_add_event_cb(btn, btn_press_cb, LV_EVENT_PRESSED, NULL);
            lv_obj_add_event_cb(btn, btn_release_cb, LV_EVENT_RELEASED, NULL);
            lv_obj_add_event_cb(btn, B[i].cb, LV_EVENT_CLICKED, NULL);
            if (B[i].btn_out) *B[i].btn_out = btn;

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, B[i].txt);
            lv_obj_set_style_text_color(lbl,
                B[i].ghost ? UI_ACCENT : lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, ui_font_text(), LV_PART_MAIN);
            lv_obj_center(lbl);
            if (B[i].lbl_out) *B[i].lbl_out = lbl;
        }
    }

    /* ═══════════════════════════════════════════════════════════════
     * 4. WiFi 扫描列表 (重叠层)
     * ═══════════════════════════════════════════════════════════════ */
    /* Overlays the WiFi input card; holds up to 4 rows (46px) + padding,
     * no scrolling — 4*46 + 8 = 192 ≤ col_h (226). */
    g_wifi_list = lv_obj_create(root);
    lv_obj_set_size(g_wifi_list, hw, col_h);
    lv_obj_set_pos(g_wifi_list, 12, col_y);  /* over the WiFi card */
    lv_obj_set_style_bg_color(g_wifi_list, UI_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_wifi_list, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_wifi_list, UI_RADIUS_SM, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_wifi_list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_wifi_list, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_wifi_list, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(g_wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(g_wifi_list, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(g_wifi_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(g_wifi_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);

    /* ═══════════════════════════════════════════════════════════════
     * 5. 键盘 (屏幕层，不在 root 内)
     *    - g_kb: 文本键盘 (SSID/密码/MQTT用户/密码)
     *    - g_kb_num: 数字键盘 (IP地址输入，大按键+小数点)
     * ═══════════════════════════════════════════════════════════════ */
    {
        lv_obj_t *kb_wrap = lv_obj_create(lv_screen_active());
        lv_obj_set_size(kb_wrap, LCD_WIDTH, 260);  /* full-screen width */
        lv_obj_align(kb_wrap, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(kb_wrap, ui_color_surface(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(kb_wrap, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(kb_wrap, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(kb_wrap, 8, LV_PART_MAIN);
        lv_obj_set_style_radius(kb_wrap, 16, LV_PART_MAIN);
        lv_obj_add_flag(kb_wrap, LV_OBJ_FLAG_HIDDEN);

        /* ── 文本键盘 (默认) ──────────────────────────────────── */
        g_kb = lv_keyboard_create(kb_wrap);
        lv_obj_set_size(g_kb, LCD_WIDTH - 16, 244);
        lv_obj_set_style_bg_color(g_kb, ui_color_surface(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_kb, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_font(g_kb, &lv_font_montserrat_24, LV_PART_ITEMS);
        lv_obj_set_style_text_color(g_kb, ui_color_text(), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(g_kb, ui_color_surface_light(), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(g_kb, ui_color_surface_light(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(g_kb, ui_color_text(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(g_kb, 0, LV_PART_ITEMS);
        lv_obj_set_style_shadow_width(g_kb, 0, LV_PART_ITEMS);
        lv_obj_set_style_radius(g_kb, UI_RADIUS_SM, LV_PART_ITEMS);
        lv_obj_set_style_pad_all(g_kb, 6, LV_PART_ITEMS);
        lv_obj_add_event_cb(g_kb, kb_ok_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(g_kb, kb_ok_cb, LV_EVENT_CANCEL, NULL);
        /* ── 数字键盘 (IP地址) ────────────────────────────────── */
        g_kb_num = lv_keyboard_create(kb_wrap);
        lv_obj_set_size(g_kb_num, LCD_WIDTH - 16, 244);
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_kb_num, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_keyboard_set_mode(g_kb_num, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_set_style_text_font(g_kb_num, &lv_font_montserrat_36, LV_PART_ITEMS);
        lv_obj_set_style_text_color(g_kb_num, ui_color_text(), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface_light(), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface_light(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(g_kb_num, ui_color_text(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(g_kb_num, 0, LV_PART_ITEMS);
        lv_obj_set_style_shadow_width(g_kb_num, 0, LV_PART_ITEMS);
        lv_obj_set_style_radius(g_kb_num, UI_RADIUS_SM, LV_PART_ITEMS);
        lv_obj_set_style_pad_all(g_kb_num, 8, LV_PART_ITEMS);
        lv_obj_add_flag(g_kb_num, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(g_kb_num, kb_ok_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(g_kb_num, kb_ok_cb, LV_EVENT_CANCEL, NULL);

        g_kb_wrap = kb_wrap;
    }

    /* Backward compat */
    ui_net_theme_btn = root;

    /* 从 NVS 加载已保存的 MQTT 配置 */
    read_mqtt_settings();

    /* Remove textareas from default group — cursor only appears when user taps,
     * at which point ta_focus_cb adds them to the keyboard group. */
    lv_group_t *def_g = lv_group_get_default();
    if (def_g) {
        if (ui_net_ssid_ta)    lv_group_remove_obj(ui_net_ssid_ta);
        if (ui_net_pass_ta)    lv_group_remove_obj(ui_net_pass_ta);
        if (g_mqtt_uri_ta)     lv_group_remove_obj(g_mqtt_uri_ta);
        if (g_mqtt_user_ta)    lv_group_remove_obj(g_mqtt_user_ta);
        if (g_mqtt_pass_ta)    lv_group_remove_obj(g_mqtt_pass_ta);
    }

    ESP_LOGI(TAG, "network screen created");
    return root;
}

void ui_screen_network_on_enter(void)
{
    if (!g_wifi_list) return;
    cancel_scan();
    clean_ssid_list(g_wifi_list);
    lv_obj_add_flag(g_wifi_list, LV_OBJ_FLAG_HIDDEN);
    /* If boot scan already cached results, populate list silently so when
     * user clicks the SSID field, results appear instantly. */
    if (g_ap_cache_valid && g_ap_cache_count > 0) {
        /* Start a fresh non-blocking scan to keep results current */
        do_wifi_scan(g_wifi_list);
        populate_ap_list(g_wifi_list, g_ap_cache, g_ap_cache_count);
    }

    /* Screens are created lazily, so status labels are freshly "--" the first
     * time this screen opens.  Refresh WiFi/IP from live state (non-blocking,
     * no socket test) so an already-connected device shows correct values.
     * MQTT status stays event-driven (no getter). */
    wifi_ap_record_t ap;
    bool connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    if (ui_net_wifi_label) {
        lv_label_set_text_fmt(ui_net_wifi_label, "WiFi: %s",
                              connected ? (char *)ap.ssid : "--");
    }
    if (ui_net_ip_label) {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip_info;
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr) {
            lv_label_set_text_fmt(ui_net_ip_label, "IP: " IPSTR, IP2STR(&ip_info.ip));
        } else {
            lv_label_set_text(ui_net_ip_label, "IP: --");
        }
    }
}

void ui_screen_network_lang_update(void)
{
    if (g_wifi_title)
        lv_label_set_text(g_wifi_title, ui_lang_str(TXT_WIFI_SETUP));
    if (g_mqtt_title)
        lv_label_set_text(g_mqtt_title, ui_lang_str(TXT_MQTT_CONFIG));
    if (g_qr_url_label)
        lv_label_set_text(g_qr_url_label, ui_lang_str(TXT_SCAN_QR));
    if (g_connect_btn_lbl)
        lv_label_set_text(g_connect_btn_lbl, ui_lang_str(TXT_CONNECT));
    if (g_reset_btn_lbl)
        lv_label_set_text(g_reset_btn_lbl, ui_lang_str(TXT_RESET_WIFI));
    /* Update textarea placeholders */
    if (ui_net_ssid_ta)
        lv_textarea_set_placeholder_text(ui_net_ssid_ta, ui_lang_str(TXT_WIFI_NAME));
    if (ui_net_pass_ta)
        lv_textarea_set_placeholder_text(ui_net_pass_ta, ui_lang_str(TXT_WIFI_PASS));
    if (g_mqtt_user_ta)
        lv_textarea_set_placeholder_text(g_mqtt_user_ta, ui_lang_str(TXT_MQTT_USER));
    if (g_mqtt_pass_ta)
        lv_textarea_set_placeholder_text(g_mqtt_pass_ta, ui_lang_str(TXT_MQTT_PASS));

    /* Refresh QR URL — actual IP might be same, but just in case */
    qr_update_url();
}

void ui_screen_network_theme_update(void)
{
    /* Textarea cursor — bg=text color, text=bg color (inverted), opaque */
    if (ui_net_ssid_ta) {
        lv_obj_set_style_bg_color(ui_net_ssid_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_net_ssid_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_net_ssid_ta, UI_TEXT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ui_net_ssid_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_text_color(ui_net_ssid_ta, UI_SURFACE_LIGHT, LV_PART_CURSOR);
        lv_obj_set_style_width(ui_net_ssid_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(ui_net_ssid_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_border_color(ui_net_ssid_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }
    if (ui_net_pass_ta) {
        lv_obj_set_style_bg_color(ui_net_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui_net_pass_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_net_pass_ta, UI_TEXT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(ui_net_pass_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_text_color(ui_net_pass_ta, UI_SURFACE_LIGHT, LV_PART_CURSOR);
        lv_obj_set_style_width(ui_net_pass_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(ui_net_pass_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_border_color(ui_net_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }
    if (g_mqtt_uri_ta) {
        lv_obj_set_style_bg_color(g_mqtt_uri_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_uri_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_mqtt_uri_ta, UI_TEXT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_uri_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_text_color(g_mqtt_uri_ta, UI_SURFACE_LIGHT, LV_PART_CURSOR);
        lv_obj_set_style_width(g_mqtt_uri_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_uri_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_border_color(g_mqtt_uri_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }
    if (g_mqtt_user_ta) {
        lv_obj_set_style_bg_color(g_mqtt_user_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_user_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_mqtt_user_ta, UI_TEXT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_user_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_text_color(g_mqtt_user_ta, UI_SURFACE_LIGHT, LV_PART_CURSOR);
        lv_obj_set_style_width(g_mqtt_user_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_user_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_border_color(g_mqtt_user_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }
    if (g_mqtt_pass_ta) {
        lv_obj_set_style_bg_color(g_mqtt_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(g_mqtt_pass_ta, UI_TEXT, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_mqtt_pass_ta, UI_TEXT, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(g_mqtt_pass_ta, LV_OPA_TRANSP, LV_PART_CURSOR);
        lv_obj_set_style_text_color(g_mqtt_pass_ta, UI_SURFACE_LIGHT, LV_PART_CURSOR);
        lv_obj_set_style_width(g_mqtt_pass_ta, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_width(g_mqtt_pass_ta, 0, LV_PART_CURSOR);
        lv_obj_set_style_border_color(g_mqtt_pass_ta, UI_SURFACE_LIGHT, LV_PART_MAIN);
    }

    /* SSID list */
    if (g_wifi_list) {
        lv_obj_set_style_bg_color(g_wifi_list, UI_SURFACE, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_wifi_list, UI_ACCENT, LV_PART_MAIN);
        uint32_t n = lv_obj_get_child_cnt(g_wifi_list);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *row = lv_obj_get_child(g_wifi_list, i);
            uint32_t nc = lv_obj_get_child_cnt(row);
            for (uint32_t j = 0; j < nc; j++) {
                lv_obj_t *child = lv_obj_get_child(row, j);
                if (lv_obj_check_type(child, &lv_label_class)) {
                    const char *txt = lv_label_get_text(child);
                    if (txt && txt[0] != '\xEF')
                        lv_obj_set_style_text_color(child, UI_TEXT, LV_PART_MAIN);
                }
            }
        }
    }

    /* Walk all children of g_root to update labels and small buttons */
    if (g_root) {
        uint32_t nc = lv_obj_get_child_cnt(g_root);
        for (uint32_t i = 0; i < nc; i++) {
            lv_obj_t *card = lv_obj_get_child(g_root, i);
            if (!lv_obj_check_type(card, &lv_obj_class)) continue;
            /* Update card child labels (titles, descriptions) */
            uint32_t nk = lv_obj_get_child_cnt(card);
            for (uint32_t j = 0; j < nk; j++) {
                lv_obj_t *kid = lv_obj_get_child(card, j);
                if (lv_obj_check_type(kid, &lv_label_class)) {
                    lv_obj_set_style_text_color(kid, UI_TEXT_SECONDARY, LV_PART_MAIN);
                }
                /* Small icon buttons use UI_SURFACE_LIGHT bg */
                else if (lv_obj_check_type(kid, &lv_button_class)) {
                    lv_obj_t *btn = kid;
                    lv_coord_t bw = lv_obj_get_width(btn);
                    lv_coord_t bh = lv_obj_get_height(btn);
                    if (bw <= 44 && bh <= 44) {
                        lv_obj_set_style_bg_color(btn, UI_SURFACE_LIGHT, LV_PART_MAIN);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Status labels */
    if (ui_net_wifi_label)
        lv_obj_set_style_text_color(ui_net_wifi_label, UI_TEXT, LV_PART_MAIN);
    if (ui_net_ip_label)
        lv_obj_set_style_text_color(ui_net_ip_label, UI_TEXT_MUTED, LV_PART_MAIN);
    if (ui_net_mqtt_label)
        lv_obj_set_style_text_color(ui_net_mqtt_label, UI_TEXT_MUTED, LV_PART_MAIN);
    if (g_qr_url_label)
        lv_obj_set_style_text_color(g_qr_url_label, UI_TEXT_MUTED, LV_PART_MAIN);
    if (g_mqtt_status_label)
        lv_obj_set_style_text_color(g_mqtt_status_label, UI_TEXT_SECONDARY, LV_PART_MAIN);

    /* Keyboard wrapper + keyboards */
    if (g_kb_wrap)
        lv_obj_set_style_bg_color(g_kb_wrap, ui_color_surface(), LV_PART_MAIN);
    if (g_kb) {
        lv_obj_set_style_bg_color(g_kb, ui_color_surface(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_kb, ui_color_surface_light(), LV_PART_ITEMS);
        lv_obj_set_style_text_color(g_kb, ui_color_text(), LV_PART_ITEMS);
        lv_obj_set_style_border_width(g_kb, 0, LV_PART_ITEMS);
        lv_obj_set_style_shadow_width(g_kb, 0, LV_PART_ITEMS);
        /* Checked buttons (arrows, space, control toggles) need explicit styling
         * otherwise they fall back to LVGL's default dark theme */
        lv_obj_set_style_bg_color(g_kb, ui_color_surface_light(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(g_kb, ui_color_text(), LV_PART_ITEMS | LV_STATE_CHECKED);
    }
    if (g_kb_num) {
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface_light(), LV_PART_ITEMS);
        lv_obj_set_style_text_color(g_kb_num, ui_color_text(), LV_PART_ITEMS);
        lv_obj_set_style_border_width(g_kb_num, 0, LV_PART_ITEMS);
        lv_obj_set_style_shadow_width(g_kb_num, 0, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(g_kb_num, ui_color_surface_light(), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(g_kb_num, ui_color_text(), LV_PART_ITEMS | LV_STATE_CHECKED);
    }
}
