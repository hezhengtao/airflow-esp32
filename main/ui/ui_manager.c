#include "ui_manager.h"
#include <stdio.h>
#include "ui_theme.h"
#include "ui_lang.h"
#include "ui_apple_anim.h"
#include "ui_boot.h"
#include "ui_design.h"
#include "ui_screen_home.h"
#include "ui_screen_settings.h"
#include "ui_screen_network.h"
#include "ui_screen_power.h"
#include "ui_screen_sound.h"

#include "ui_provision_prompt.h"

#include "board.h"
#include "lcd_tk043f1509.h"
#include "app_controller.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"

#include "esp_lcd_io_i2c.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_touch_ft5x06.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "lcd_tk043f1509.h"
#include "wifi_prov.h"
#include "app_controller.h"
#include "settings.h"
#include "speaker.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <stdlib.h>

static const char *TAG = "ui_mgr";

static void power_action_poll_cb(lv_timer_t *t);
static void holiday_timer_cb(lv_timer_t *t);

static ui_screen_t g_current = UI_SCREEN_HOME;
static ui_screen_t g_home_screen = UI_SCREEN_HOME;
static lv_disp_t *g_disp = NULL;

/* ── Screens ─────────────────────────────────────────────────────── */
static lv_obj_t *g_screens[UI_SCREEN_COUNT] = {NULL};

/* ── Left icon navigation rail ───────────────────────────────────── */
static lv_obj_t *g_nav_rail = NULL;
static lv_obj_t *g_nav_btns[UI_SCREEN_COUNT] = {NULL};
static lv_obj_t *g_nav_icons[UI_SCREEN_COUNT] = {NULL};
static lv_obj_t *g_nav_marker = NULL;      /* accent bar beside active icon */

/* Tap tracking (double-tap wake + click sound) */
static int32_t g_swipe_x = 0;
static int32_t g_swipe_y = 0;
static bool g_swipe_active = false;
static uint32_t g_swipe_block_until = 0;  /* block dangerous clicks for 500ms after nav */

/* Screen-off / double-tap wake */
static bool g_screen_off = false;
static bool g_shutdown = false;
static bool g_waking = false;     /* guard: wake already in progress (boot anim playing) */
static uint32_t g_last_tap_ms = 0;
#define DOUBLE_TAP_WINDOW_MS  500

static void nav_update_active(int screen);

/* Lazily create a screen the first time it's shown.  Returns the (cached)
 * root object.  Keeps only the visited screens in RAM instead of all 5. */
static lv_obj_t *ensure_screen(int idx)
{
    if (idx < 0 || idx >= UI_SCREEN_COUNT) return NULL;
    if (g_screens[idx]) return g_screens[idx];

    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *o = NULL;
    switch (idx) {
        case UI_SCREEN_HOME:     o = ui_screen_home_create(scr);     break;
        case UI_SCREEN_NETWORK:  o = ui_screen_network_create(scr);  break;
        case UI_SCREEN_SETTINGS: o = ui_screen_settings_create(scr); break;
        case UI_SCREEN_SOUND:    o = ui_screen_sound_create(scr);    break;
        case UI_SCREEN_POWER:    o = ui_screen_power_create(scr);    break;
        default: return NULL;
    }
    g_screens[idx] = o;
    /* Start hidden+transparent — do_transition will unhide and set opaque.
     * IMPORTANT: only use opa, NOT FLAG_HIDDEN, because hiding/re-showing
     * can break event delivery to lazy-created screens in LVGL v9. */
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
    /* Keep nav rail on top so it still draws over the transparent new screen */
    if (g_nav_rail) lv_obj_move_foreground(g_nav_rail);
    return o;
}

/* Instant switch — one frame, no animation. Crossfade would render two
 * screens in partial-buffer mode, causing tearing and visible flicker. */
static void do_transition(int from, int to)
{
    if (from == to) return;

    /* Create the target screen on first visit */
    ensure_screen(to);
    if (!g_screens[to]) return;

    /* Dismiss keyboard if leaving network screen */
    if (from == UI_SCREEN_NETWORK) ui_screen_network_hide_keyboard();

    lv_obj_add_flag(g_screens[from], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(g_screens[to], UI_CONTENT_X, 0);
    lv_obj_set_style_opa(g_screens[to], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(g_screens[to], LV_OBJ_FLAG_HIDDEN);
    g_current = (ui_screen_t)to;

    /* Keep the nav rail above the newly-shown screen and refresh highlight */
    if (g_nav_rail) lv_obj_move_foreground(g_nav_rail);
    nav_update_active(to);

    if (to == UI_SCREEN_NETWORK) ui_screen_network_on_enter();

    /* Mark the whole screen dirty so the new page fully repaints (clears any
     * residue from the previous page).  Do NOT call lv_refr_now() here — this
     * runs inside an LVGL input callback, and forcing a synchronous refresh
     * re-enters the draw pipeline (draw-layer alloc + TLSF), corrupting the
     * heap and crashing after a few swaps.  The normal LVGL refresh timer
     * will repaint the invalidated area on the next cycle. */
    lv_obj_invalidate(lv_screen_active());
}

/* ── Left icon navigation rail ───────────────────────────────────── */

/* Screen order must match ui_screen_t / g_screens[] */
static const char *NAV_ICONS[UI_SCREEN_COUNT] = {
    LV_SYMBOL_HOME,      /* UI_SCREEN_HOME    */
    LV_SYMBOL_WIFI,      /* UI_SCREEN_NETWORK */
    LV_SYMBOL_SETTINGS,  /* UI_SCREEN_SETTINGS*/
    LV_SYMBOL_AUDIO,     /* UI_SCREEN_SOUND   */
    LV_SYMBOL_POWER,     /* UI_SCREEN_POWER   */
};

/* Highlight the active icon and slide the accent marker beside it */
static void nav_update_active(int screen)
{
    if (!g_nav_rail) return;
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (!g_nav_icons[i]) continue;
        bool active = (i == screen);
        lv_obj_set_style_text_color(g_nav_icons[i],
                                    active ? UI_ACCENT : ui_color_text_muted(),
                                    LV_PART_MAIN);
        if (g_nav_btns[i]) {
            lv_obj_set_style_bg_opa(g_nav_btns[i],
                                    active ? LV_OPA_20 : LV_OPA_TRANSP,
                                    LV_PART_MAIN);
        }
    }
    if (g_nav_marker && screen >= 0 && screen < UI_SCREEN_COUNT && g_nav_btns[screen]) {
        lv_obj_remove_flag(g_nav_marker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(g_nav_marker, g_nav_btns[screen],
                        LV_ALIGN_OUT_LEFT_MID, 4, 0);
    }
}

static void nav_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= UI_SCREEN_COUNT) return;
    if (idx == (int)g_current) return;
    do_transition((int)g_current, idx);
    /* click sound is played by indev_release_cb on the tap release;
     * brief guard so dangerous actions can't fire right after nav */
    g_swipe_block_until = lv_tick_get() + 300;
}

/* Build the persistent left rail on the active screen (above all screens) */
static void nav_rail_create(lv_obj_t *parent)
{
    g_nav_rail = lv_obj_create(parent);
    lv_obj_set_size(g_nav_rail, UI_NAV_W, UI_CONTENT_H);
    lv_obj_set_pos(g_nav_rail, 0, 0);
    /* Flat: blend with the screen background, separated only by a hairline */
    lv_obj_set_style_bg_color(g_nav_rail, ui_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_nav_rail, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_nav_rail, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(g_nav_rail, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_nav_rail, ui_color_surface_light(), LV_PART_MAIN);
    lv_obj_set_style_radius(g_nav_rail, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_nav_rail, 0, LV_PART_MAIN);
    lv_obj_remove_flag(g_nav_rail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_nav_rail, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(g_nav_rail, LV_SCROLLBAR_MODE_OFF);

    /* Evenly distribute 5 icon buttons down the rail */
    const int btn_sz = 48;
    const int slot_h = UI_CONTENT_H / UI_SCREEN_COUNT;   /* 96 */
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        lv_obj_t *btn = lv_obj_create(g_nav_rail);
        lv_obj_set_size(btn, btn_sz, btn_sz);
        int cx = (UI_NAV_W - btn_sz) / 2;
        int cy = i * slot_h + (slot_h - btn_sz) / 2;
        lv_obj_set_pos(btn, cx, cy);
        lv_obj_set_style_radius(btn, UI_RADIUS_BTN, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, UI_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);

        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, NAV_ICONS[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, ui_color_text_muted(), LV_PART_MAIN);
        lv_obj_center(icon);

        g_nav_btns[i] = btn;
        g_nav_icons[i] = icon;
    }

    /* Accent marker bar shown beside the active icon */
    g_nav_marker = lv_obj_create(g_nav_rail);
    lv_obj_set_size(g_nav_marker, 4, 40);
    lv_obj_set_style_bg_color(g_nav_marker, UI_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_nav_marker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_nav_marker, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_nav_marker, 2, LV_PART_MAIN);
    lv_obj_set_pos(g_nav_marker, 0, 0);
    lv_obj_add_flag(g_nav_marker, LV_OBJ_FLAG_HIDDEN);
}

/* Refresh rail colors after a theme change */
static void nav_rail_theme_update(void)
{
    if (!g_nav_rail) return;
    lv_obj_set_style_bg_color(g_nav_rail, ui_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_nav_rail, ui_color_surface_light(), LV_PART_MAIN);
    nav_update_active((int)g_current);
}

/* ── Touch input device ──────────────────────────────────────────── */

static lv_indev_t *g_touch_indev = NULL;

/* ── Tap tracking ─────────────────────────────────────────────────── */

static void indev_press_cb(lv_event_t *e)
{
    /* Screen navigation is via the left icon rail (nav_btn_cb).
     * Here just record the press for double-tap wake + click sound. */
    if (!g_touch_indev) return;

    lv_point_t pt;
    lv_indev_get_point(g_touch_indev, &pt);
    g_swipe_x = pt.x;
    g_swipe_y = pt.y;
    g_swipe_active = true;
}

static void indev_release_cb(lv_event_t *e)
{
    if (!g_touch_indev) return;
    g_swipe_active = false;

    /* Screen-off: double-tap to wake */
    if (g_screen_off) {
        uint32_t now = lv_tick_get();
        if (now - g_last_tap_ms < DOUBLE_TAP_WINDOW_MS) {
            g_last_tap_ms = 0;
            ui_manager_wake();
        } else {
            g_last_tap_ms = now;
        }
        return;
    }

    /* Click sound on non-swipe taps */
    lv_point_t pt;
    lv_indev_get_point(g_touch_indev, &pt);
    int32_t dx = pt.x - g_swipe_x, dy = pt.y - g_swipe_y;
    if ((dx > -5 && dx < 5) && (dy > -5 && dy < 5))
        speaker_click();
}

/* ── Fail-safe: one-shot fallback if boot timer never fires ──────── */

static lv_timer_t *g_failsafe_timer = NULL;

static void failsafe_show_cb(lv_timer_t *t)
{
    ESP_LOGW(TAG, "FAILSAFE: forcing ui_manager_show()");
    g_failsafe_timer = NULL;
    ui_manager_show();
}

/* ── Theme application ────────────────────────────────────────────── */

/* ── WiFi connection tracking ─────────────────────────────────────── */
static bool g_net_connected = false;

void ui_manager_apply_theme(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, ui_color_bg(), LV_PART_MAIN);

    /* Update screen roots */
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (g_screens[i]) {
            lv_obj_set_style_bg_color(g_screens[i], ui_color_bg(), LV_PART_MAIN);
            lv_obj_invalidate(g_screens[i]);
        }
    }
    lv_obj_invalidate(scr);

    /* Refresh theme-sensitive local styles in all screens */
    nav_rail_theme_update();
    ESP_LOGI(TAG, "apply_theme: home");
    ui_screen_home_theme_update();
    ESP_LOGI(TAG, "apply_theme: network");
    ui_screen_network_theme_update();
    ESP_LOGI(TAG, "apply_theme: settings");
    ui_screen_settings_theme_update();
    ESP_LOGI(TAG, "apply_theme: power");
    ui_screen_power_theme_update();
    ESP_LOGI(TAG, "apply_theme: sound");
    ui_screen_sound_theme_update();
    ESP_LOGI(TAG, "apply_theme: done");
}

/* ── Init ─────────────────────────────────────────────────────────── */

void ui_manager_init(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "Initializing LVGL UI manager");

    /* ── LVGL port ─────────────────────────────────────────────── */
    /* CRITICAL: task_stack_caps MUST NOT include MALLOC_CAP_SPIRAM.
     * NVS/flash write operations (triggered by LVGL button callbacks)
     * call spi_flash_disable_interrupts_caches_and_other_cpu() which
     * asserts that the current task stack is NOT in PSRAM.  If the
     * LVGL task stack were in PSRAM, disabling cache would make the
     * stack itself inaccessible → instant crash. */
    lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 8,
        .task_stack = 40960,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
        .task_stack_caps = 0,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    /* ── Touch I2C bus (50 kHz for FT6336 compatibility) ─────────── */
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    esp_lcd_panel_io_handle_t tp_io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));

    /*
     * Touch: panel is native 800×480 landscape (no MV/MX).  FT5x06 reports
     * X = long axis (0..799), Y = short axis (0..479).  The panel inverts X
     * (tap left → right side reacts), so mirror_x=1 fixes it.  No swap needed.
     */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_WIDTH - 1,    /* 799 — long axis → LVGL X */
        .y_max = LCD_HEIGHT - 1,   /* 479 — short axis → LVGL Y */
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 0 },
    };
    esp_lcd_touch_handle_t touch_handle = NULL;
    esp_err_t touch_err = esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &touch_handle);
    if (touch_err == ESP_OK) {
        ESP_LOGI(TAG, "FT5x06 touch probe OK");

        /* ── Touch diag: identify chip type ───────────────────────── */
        uint8_t mode = 0, firmid = 0, chipid = 0;
        esp_lcd_panel_io_rx_param(tp_io, 0x00, &mode, 1);
        esp_lcd_panel_io_rx_param(tp_io, 0xA6, &firmid, 1);
        esp_lcd_panel_io_rx_param(tp_io, 0xA8, &chipid, 1);
        ESP_LOGI(TAG, "Touch chip: mode=0x%02X firmid=0x%02X chipid=0x%02X",
                 mode, firmid, chipid);

        /* ── Enable direct I2C for FT6336 ───────────────────────────
         * FT6336 requires STOP between register-write and data-read.
         * esp_lcd_panel_io_rx_param uses repeated START (no STOP),
         * which causes 0xFF reads when the slave doesn't respond.
         * Direct I2C uses i2c_master_transmit + i2c_master_receive
         * (separate transactions = STOP in between).
         * At 100 kHz, 50 ms timeout is generous enough for Monitor mode. */
        i2c_device_config_t touch_dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = TOUCH_I2C_ADDR,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t touch_i2c_dev = NULL;
        esp_err_t i2c_err = i2c_master_bus_add_device(i2c_bus, &touch_dev_cfg, &touch_i2c_dev);
        if (i2c_err == ESP_OK) {
            esp_lcd_touch_ft5x06_set_i2c_dev(touch_handle, touch_i2c_dev);
            ESP_LOGI(TAG, "FT5x06 direct I2C enabled (separate tx/rx for FT6336)");
        } else {
            ESP_LOGW(TAG, "Cannot register direct I2C device: %s — using fallback", esp_err_to_name(i2c_err));
        }
    } else {
        ESP_LOGE(TAG, "FT5x06 probe FAILED: %s (0x%X)",
                 esp_err_to_name(touch_err), touch_err);
    }

    /* ── Take LVGL lock ────────────────────────────────────────── */
    lvgl_port_lock(0);

    /* ── Display: 800×480 native landscape (NO SW or HW rotation) ──
     * LVGL renders landscape natively (hres=800, vres=480).  The panel is
     * left in its power-on orientation (MADCTL = just BGR, no MV/MX) so
     * the i80 DMA flush writes directly row-by-row without transformation.
     * No SW-rotation buffer is needed — keeps internal DRAM free. */
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_WIDTH * 8,
        .double_buffer = false,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,    /* native landscape — no rotation */
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = { .buff_dma = true, .swap_bytes = 1 },
    };
    g_disp = lvgl_port_add_disp(&disp_cfg);
    if (!g_disp) {
        ESP_LOGE(TAG, "FATAL: lvgl_port_add_disp failed — check SRAM");
        abort();
    }
    ESP_LOGI(TAG, "Display: %dx%d landscape (native, no rotation)", LCD_WIDTH, LCD_HEIGHT);

    /* Active screen background — start black to avoid flash before boot overlay */
    {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    }

    /* ── Theme ─────────────────────────────────────────────────── */
    ui_theme_init();

    /* ── Language ──────────────────────────────────────────────── */
    ui_lang_init();

    /* ── Sound settings ───────────────────────────────────────── */
    ui_screen_sound_load_settings();

    /* ── Touch indev ───────────────────────────────────────────── */
    if (touch_handle) {
        lvgl_port_touch_cfg_t lvgl_touch_cfg = {
            .disp = g_disp,
            .handle = touch_handle,
        };
        g_touch_indev = lvgl_port_add_touch(&lvgl_touch_cfg);
    }
    if (g_touch_indev) {
        /* Read touch at 25 Hz — lower rate reduces tearing during slider drag.
         * The driver is I2C at 100 kHz; each read is ~1 ms, so 40 ms is conservative. */
        lv_timer_set_period(lv_indev_get_read_timer(g_touch_indev), 40);

        /* Indev-level swipe handlers */
        lv_indev_add_event_cb(g_touch_indev, indev_press_cb,
                              LV_EVENT_PRESSED, NULL);
        lv_indev_add_event_cb(g_touch_indev, indev_release_cb,
                              LV_EVENT_RELEASED, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to add touch indev");
    }

    /* ── Create all screens ──────────────────────────────────── */
    {
        lv_obj_t *scr = lv_screen_active();

        /* Read configured home screen from NVS */
        uint32_t hs = UI_SCREEN_HOME;
        settings_get_u32(NVS_KEY_HOME_SCREEN, &hs);
        if (hs >= UI_SCREEN_COUNT) hs = UI_SCREEN_HOME;
        g_home_screen = (ui_screen_t)hs;

        /* ── Lazy screen creation ──────────────────────────────────
         * Creating all 5 screens up-front exhausts RAM (LVGL pool vs.
         * system heap for WiFi/HTTP fight over internal SRAM).  Instead
         * build ONLY the configured home screen now; the others are
         * created on first navigation (ensure_screen).  Schedule logic
         * that must run regardless is initialised separately below. */
        g_screens[g_home_screen] = ensure_screen((int)g_home_screen);
        lv_obj_remove_flag(g_screens[g_home_screen], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g_screens[g_home_screen], UI_CONTENT_X, 0);
        lv_obj_set_style_opa(g_screens[g_home_screen], LV_OPA_TRANSP, LV_PART_MAIN);

        /* Scheduled power on/off must work even if Power screen never opens */
        ui_screen_power_init_schedule();

        /* Persistent left icon nav rail — sits above all screens */
        vTaskDelay(pdMS_TO_TICKS(1));
        nav_rail_create(scr);
        vTaskDelay(pdMS_TO_TICKS(1));
        nav_update_active((int)g_home_screen);

        /* NOTE: screens stay visible — boot overlay covers them during anim */
    }

    /* Fail-safe: one-shot fallback if boot timer never fires */
    g_failsafe_timer = lv_timer_create(failsafe_show_cb, 6000, NULL);
    lv_timer_set_repeat_count(g_failsafe_timer, 1);

    /* Periodic holiday refresh */
    lv_timer_create(holiday_timer_cb, 30000, NULL);

    /* Poll for web/MQTT power action requests */
    lv_timer_create(power_action_poll_cb, 200, NULL);

    lvgl_port_unlock();

    g_current = g_home_screen;
    ESP_LOGI(TAG, "UI manager initialized (6 screens, home=%d)", g_home_screen);
}

/* ── Deferred provision prompt ────────────────────────────────────── */
static bool g_prov_prompt_shown = false;

static void deferred_provision_cb(lv_timer_t *t)
{
    /* Don't show provision prompt while screen is off — retry later */
    if (g_screen_off) {
        return;
    }
    if (!g_net_connected) {
        if (!g_prov_prompt_shown) {
            ESP_LOGI(TAG, "WiFi not connected — showing provision prompt");
            ui_boot_hide();
            ui_provision_prompt_create(lv_screen_active());
            g_prov_prompt_shown = true;
        }
        /* Keep timer running — will retry every 1.5s until WiFi connects */
    } else {
        ESP_LOGI(TAG, "WiFi connected — showing first screen");
        if (g_prov_prompt_shown) {
            ui_provision_prompt_dismiss_inner();
        }
        ui_manager_show_first_screen();
        g_prov_prompt_shown = false;
        lv_timer_delete(t);
    }
}

/* ── Screen-off / Shutdown / Wake ─────────────────────────────────── */

static lv_obj_t *g_off_overlay = NULL;

bool ui_manager_is_swipe_blocked(void)
{
    return (lv_tick_get() < g_swipe_block_until);
}

/* Poll for web/MQTT power action requests (runs in LVGL task) */
static void power_action_poll_cb(lv_timer_t *t)
{
    power_action_t action = app_controller_consume_power_action();
    if (action == POWER_ACTION_SCREEN_OFF) {
        ui_manager_screen_off();
    } else if (action == POWER_ACTION_SHUTDOWN) {
        ui_manager_shutdown();
    } else if (action == POWER_ACTION_WAKE) {
        ui_manager_wake();
    }
}

/* Screen-off animation timer callback — cleanup spinner then backlight off */
static void screen_off_finish_cb(lv_timer_t *t)
{
    if (g_off_overlay) {
        lv_obj_clean(g_off_overlay);  /* remove spinner children */
    }
    app_controller_screen_off();
    ESP_LOGI(TAG, "screen off — double-tap to wake");
}

/* Enter "screen off" mode — spinner animation, then backlight off */
void ui_manager_screen_off(void)
{
    if (ui_manager_is_swipe_blocked()) return;
    if (g_screen_off) return;
    g_screen_off = true;
    g_last_tap_ms = 0;

    /* Cover everything with a black full-screen overlay. */
    g_off_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_off_overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g_off_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_off_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_off_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_move_foreground(g_off_overlay);

    /* Black out the screen itself too */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    /* Hide all screen content */
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (g_screens[i]) lv_obj_add_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Animated spinner — same style as shutdown animation */
    lv_obj_t *spinner = lv_spinner_create(g_off_overlay);
    lv_obj_set_size(spinner, 64, 64);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x4FC3F7), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 5, LV_PART_MAIN);
    lv_obj_center(spinner);

    /* Delay then turn off backlight — double-tap during animation still wakes */
    lv_timer_t *t = lv_timer_create(screen_off_finish_cb, 800, NULL);
    lv_timer_set_repeat_count(t, 1);
}

/* Shutdown: spinner + large colored text + double-tap to cancel/wake */

static lv_obj_t *g_sd_spinner = NULL;
static lv_obj_t *g_sd_label = NULL;
static lv_timer_t *g_sd_timer = NULL;

static void sd_cleanup_label_cb(lv_timer_t *t)
{
    g_sd_timer = NULL;
    if (g_sd_label) {
        lv_obj_delete(g_sd_label);
        g_sd_label = NULL;
    }
    if (g_sd_spinner) {
        lv_obj_delete(g_sd_spinner);
        g_sd_spinner = NULL;
    }
    /* Now turn off backlight — animation is fully finished */
    app_controller_screen_off();
}

static void shutdown_exec_cb(lv_timer_t *t)
{
    g_sd_timer = NULL;

    /* Stop spinner and remove label immediately */
    if (g_sd_spinner) {
        lv_obj_delete(g_sd_spinner);
        g_sd_spinner = NULL;
    }
    if (g_sd_label) {
        lv_obj_delete(g_sd_label);
        g_sd_label = NULL;
    }

    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (g_screens[i]) lv_obj_add_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    app_controller_shutdown();

    /* Turn off backlight after a short delay */
    g_sd_timer = lv_timer_create(sd_cleanup_label_cb, 800, NULL);
    lv_timer_set_repeat_count(g_sd_timer, 1);

    ESP_LOGI(TAG, "shutdown complete");
}

void ui_manager_shutdown(void)
{
    if (ui_manager_is_swipe_blocked()) return;
    if (g_screen_off) return;
    g_screen_off = true;
    g_shutdown = true;
    g_last_tap_ms = 0;

    /* Play power-off sound */
    speaker_power_off();

    /* Full black overlay (backlight stays on for animation) */
    g_off_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_off_overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_pos(g_off_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_off_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_off_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_off_overlay, 0, LV_PART_MAIN);
    lv_obj_move_foreground(g_off_overlay);

    /* Spinner — rotating arc, accent color */
    g_sd_spinner = lv_spinner_create(g_off_overlay);
    lv_obj_set_size(g_sd_spinner, 80, 80);
    lv_obj_set_style_arc_color(g_sd_spinner, lv_color_hex(0x4FC3F7), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_sd_spinner, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_sd_spinner, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_sd_spinner, 6, LV_PART_MAIN);
    lv_obj_align(g_sd_spinner, LV_ALIGN_CENTER, 0, -40);

    /* Accent-colored "关机中" label below spinner */
    g_sd_label = lv_label_create(g_off_overlay);
    lv_label_set_text(g_sd_label, ui_lang_str(TXT_SHUTTING_DOWN));
    lv_obj_set_style_text_color(g_sd_label, lv_color_hex(0x4FC3F7), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_sd_label, ui_font_text(), LV_PART_MAIN);
    lv_obj_align(g_sd_label, LV_ALIGN_CENTER, 0, 50);

    /* Delay then execute shutdown */
    g_sd_timer = lv_timer_create(shutdown_exec_cb, 2000, NULL);
    lv_timer_set_repeat_count(g_sd_timer, 1);

    ESP_LOGI(TAG, "shutdown started");
}

/* Wake from shutdown: boot-animation callback — always go to home screen */
static void wake_after_boot(void)
{
    ui_boot_hide();  /* remove boot overlay */
    g_screen_off = false;
    app_controller_wake();  /* clear shutdown guard so alarm/rpm tasks can run */
    g_waking = false;
    ui_manager_apply_theme();

    /* Force home screen — hide all others (some may be NULL: lazy-created) */
    int hs = (int)g_home_screen;
    ensure_screen(hs);
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (!g_screens[i]) continue;
        if (i == hs) {
            lv_obj_remove_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g_screens[i], UI_CONTENT_X, 0);
            lv_obj_set_style_opa(g_screens[i], LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_add_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    g_current = g_home_screen;
    if (g_nav_rail) lv_obj_move_foreground(g_nav_rail);
    nav_update_active(hs);

    ESP_LOGI(TAG, "wake from shutdown → screen %d", hs);
}

/* Wake: remove black overlay, restore UI (handles screen-off and shutdown) */
void ui_manager_wake(void)
{
    /* Guard against re-entrant call while boot animation is playing.
     * The 30s schedule timer fires twice per target minute; the second
     * call would see g_shutdown==false and take the wrong path. */
    if (g_waking) {
        ESP_LOGW(TAG, "wake already in progress, ignoring");
        return;
    }

    /* Cancel any pending shutdown timer */
    if (g_shutdown) {
        g_waking = true;
        g_shutdown = false;
        if (g_sd_timer) {
            lv_timer_delete(g_sd_timer);
            g_sd_timer = NULL;
        }
        g_sd_spinner = NULL;
        g_sd_label = NULL;
        /* Play power-on melody */
        speaker_power_on();
        ESP_LOGI(TAG, "waking from shutdown with boot animation");

        /* Delete shutdown overlay */
        if (g_off_overlay) {
            lv_obj_delete(g_off_overlay);
            g_off_overlay = NULL;
        }
        /* Turn on backlight (boot_light_on_cb also sets it) */
        app_controller_screen_on();
        /* Play boot animation → wake_after_boot restores UI when done */
        ui_boot_show(lv_screen_active(), wake_after_boot);
        return;
    }

    /* Regular screen-off wake (instant, no boot animation) */
    g_screen_off = false;
    app_controller_screen_on();

    if (g_off_overlay) {
        lv_obj_delete(g_off_overlay);
        g_off_overlay = NULL;
    }

    ui_manager_apply_theme();
    lv_obj_t *cur = g_screens[g_current];
    if (cur) {
        lv_obj_set_pos(cur, UI_CONTENT_X, 0);
        lv_obj_set_style_opa(cur, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(cur, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_nav_rail) lv_obj_move_foreground(g_nav_rail);
    nav_update_active((int)g_current);

    ESP_LOGI(TAG, "wake done");
}

/* ── Show after boot animation ────────────────────────────────────── */

void ui_manager_show_first_screen(void)
{
    int hs = (int)g_home_screen;

    lcd_display_on();

    /* ── Step 1: Make the home screen fully visible BEFORE removing the
     *     boot overlay.  If we delete the overlay first, the transparent
     *     home screen would expose the black active-screen background. ── */
    lv_obj_t *scr = lv_screen_active();
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        if (!g_screens[i]) continue;
        bool hid = lv_obj_has_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
        int opa = lv_obj_get_style_opa(g_screens[i], LV_PART_MAIN);
        ESP_LOGI(TAG, "  [%d] ptr=%p hidden=%d opa=%d",
                 i, (void *)g_screens[i], hid, opa);
        if (i == hs) {
            lv_obj_remove_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g_screens[i], UI_CONTENT_X, 0);
            lv_obj_set_style_opa(g_screens[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(g_screens[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_move_foreground(g_screens[i]);
            lv_obj_invalidate(g_screens[i]);
        } else {
            lv_obj_add_flag(g_screens[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    /* Keep the nav rail above the freshly-foregrounded home screen */
    if (g_nav_rail) lv_obj_move_foreground(g_nav_rail);
    nav_update_active(hs);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(scr);

    /* Force a render pass now so the home screen is already in the
     * framebuffer when we tear down the boot overlay. */
    lv_refr_now(g_disp);

    /* ── Step 2: Apply theme (changes bg colors etc.) — do this AFTER
     *     making the screen visible so the theme colors are seen. ── */
    ui_manager_apply_theme();

    /* ── Step 3: Now it's safe to remove the boot overlay.  The home
     *     screen underneath is already rendered and opaque. ── */
    ui_boot_hide();

    /* Background scan — runs in its own task, no display impact */
    ui_screen_network_boot_scan();

    /* Post-show verification */
    {
        lv_obj_t *hs_obj = g_screens[hs];
        int opa = lv_obj_get_style_opa(hs_obj, LV_PART_MAIN);
        int bg_opa = lv_obj_get_style_bg_opa(hs_obj, LV_PART_MAIN);
        bool hid = lv_obj_has_flag(hs_obj, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "post-show verify: screen %d opa=%d bg_opa=%d hidden=%d",
                 hs, opa, bg_opa, hid);
    }
}

void ui_manager_show(void)
{
    static bool shown = false;

    ESP_LOGI(TAG, "ui_manager_show: already_shown=%d net_connected=%d",
             shown, g_net_connected);
    if (shown) return;
    shown = true;

    if (g_failsafe_timer) {
        lv_timer_delete(g_failsafe_timer);
        g_failsafe_timer = NULL;
    }

    lv_obj_invalidate(lv_screen_active());

    /* If WiFi is already connected, skip the 1.5s deferred wait and show
     * the home screen immediately — avoids an unnecessary black screen. */
    if (g_net_connected) {
        ESP_LOGI(TAG, "WiFi already connected — showing first screen now");
        ui_manager_show_first_screen();
    } else {
        lv_timer_t *pt = lv_timer_create(deferred_provision_cb, 1500, NULL);
        lv_timer_set_repeat_count(pt, -1);  /* run until WiFi connects */
        ESP_LOGI(TAG, "boot transition done — waiting for provision check");
    }
}

/* ── Navigation ───────────────────────────────────────────────────── */

void ui_manager_navigate_to(ui_screen_t screen)
{
    if (screen >= UI_SCREEN_COUNT) return;
    if (screen == g_current) return;
    do_transition((int)g_current, (int)screen);
}

ui_screen_t ui_manager_get_current(void) { return g_current; }

/* ── Data update functions ────────────────────────────────────────── */

void ui_update_temperature(float temp_c)
{
    lvgl_port_lock(0);
    ui_home_update_temp(temp_c);
    lvgl_port_unlock();
}

void ui_update_humidity(float humidity_pct)
{
    /* No humidity widget on the current home screen */
    (void)humidity_pct;
}

void ui_update_tvoc(uint16_t tvoc_ugm3)
{
    lvgl_port_lock(0);
    ui_home_update_tvoc(tvoc_ugm3);
    lvgl_port_unlock();
}

void ui_update_co2(uint16_t co2_ppm)
{
    lvgl_port_lock(0);
    ui_home_update_co2(co2_ppm);
    lvgl_port_unlock();
}

void ui_update_ch2o(uint16_t ch2o_ugm3)
{
    lvgl_port_lock(0);
    ui_home_update_ch2o(ch2o_ugm3);
    lvgl_port_unlock();
}


void ui_update_fan_speed(uint8_t speed_pct)
{
    lvgl_port_lock(0);
    ui_home_update_fan_speed(speed_pct);
    lvgl_port_unlock();
}

void ui_update_fan_rpm(uint16_t rpm)
{
    lvgl_port_lock(0);
    ui_home_update_fan_rpm(rpm);
    lvgl_port_unlock();
}

void ui_update_fan_state(bool on)
{
    lvgl_port_lock(0);
    lvgl_port_unlock();
}

void ui_update_wifi_status(bool connected, const char *ssid)
{
    g_net_connected = connected;
    lvgl_port_lock(0);
    if (ui_net_wifi_label) {
        lv_label_set_text_fmt(ui_net_wifi_label, ui_lang_str(TXT_SSID_FMT),
            connected ? ssid : ui_lang_str(TXT_OFFLINE));
    }
    qr_update_url();
    lvgl_port_unlock();
}

void ui_update_mqtt_status(bool connected)
{
    lvgl_port_lock(0);
    if (ui_net_mqtt_label) {
        lv_label_set_text(ui_net_mqtt_label,
            connected ? ui_lang_str(TXT_MQTT_CONNECTED)
                      : ui_lang_str(TXT_MQTT_OFFLINE));
    }
    lvgl_port_unlock();
}

void ui_update_ip(const char *ip)
{
    lvgl_port_lock(0);
    if (ui_net_ip_label) {
        lv_label_set_text_fmt(ui_net_ip_label, ui_lang_str(TXT_IP_FMT), ip);
    }
    qr_update_url();
    lvgl_port_unlock();
}

/* ── Holiday timer ────────────────────────────────────────────────── */

static void holiday_timer_cb(lv_timer_t *t)
{
    ui_home_update_holiday();
}

/* ── Alert ─────────────────────────────────────────────────────────── */

void ui_show_alert(const char *title, const char *message)
{
    lvgl_port_lock(0);
    lv_obj_t *msgbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(msgbox, title);
    lv_msgbox_add_text(msgbox, message);
    lv_msgbox_add_footer_button(msgbox, ui_lang_str(TXT_OK));
    lv_obj_center(msgbox);
    lvgl_port_unlock();
}
