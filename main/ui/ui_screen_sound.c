#include "ui_screen_sound.h"
#include "ui_design.h"
#include "ui_theme.h"
#include "ui_lang.h"
#include "board.h"
#include "speaker.h"
#include "app_controller.h"
#include "esp_log.h"

#define TAG "sound"
#define MELODY_MAX_IDX 5

static lv_obj_t *g_title[3];
static lv_obj_t *g_switch[3];
static lv_obj_t *g_slider[3];
static lv_obj_t *g_pct[3];

/* ── Switch ─────────────────────────────────────────────────────── */
static void sw_k_cb(lv_event_t *e) {
    app_controller_set_key_sound(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}
static void sw_p_cb(lv_event_t *e) {
    app_controller_set_power_sound(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}
static void sw_a_cb(lv_event_t *e) {
    app_controller_set_alarm_sound(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

/* ── Volume slider + pct ────────────────────────────────────────── */
static void vol_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    lv_obj_t *pct = lv_event_get_user_data(e);
    int v = lv_slider_get_value(sl);
    lv_label_set_text_fmt(pct, "%d%%", v);
}

/* ── Melody stepper ─────────────────────────────────────────────── */
typedef struct { lv_obj_t *num; uint8_t idx; void (*f)(uint8_t); void (*p)(uint8_t); } mctx_t;
/* Melody names — CN (matches web page) + EN */
static const char *MEL_CN_KEY[]   = {"马里奥金币","短促滴答","水滴","电子哔","轻声滴答","双击"};
static const char *MEL_CN_PON[]   = {"马里奥主题","上行音阶","叮咚门铃","大三和弦","小星星","号角"};
static const char *MEL_CN_POFF[]  = {"下行GEC","长下行音阶","叮咚下行","小调琶音","再见","渐慢风落"};
static const char *MEL_CN_ALARM[] = {"经典警笛","急促哔哔","高低扫频","三连脉冲","连续快滴","低频轰鸣"};
static const char **MEL_CN[] = { MEL_CN_KEY, MEL_CN_PON, MEL_CN_POFF, MEL_CN_ALARM };

static const char *MEL_EN_KEY[]   = {"Mario Coin","Short Beep","Water Drop","E-Beep","Soft Tick","Dbl Click"};
static const char *MEL_EN_PON[]   = {"Mario Theme","Ascending","Ding Dong","Major Chord","Lil Star","Fanfare"};
static const char *MEL_EN_POFF[]  = {"Descend","Long Desc","Ding Dong","Minor Arp","Goodbye","Wind Down"};
static const char *MEL_EN_ALARM[] = {"Siren","Fast Beep","Hi-Lo","Tri Pulse","Fast Drip","Low Hum"};
static const char **MEL_EN[] = { MEL_EN_KEY, MEL_EN_PON, MEL_EN_POFF, MEL_EN_ALARM };

static const char *mel_name(int cat, int idx) {
    return ui_lang_get() == LANG_CN ? MEL_CN[cat][idx] : MEL_EN[cat][idx];
}

static mctx_t g_mel[4];
static void mel_set(uint8_t i) { if (g_mel[i].f) g_mel[i].f(g_mel[i].idx); }
static void mel_play(uint8_t i) { if (g_mel[i].p) g_mel[i].p(g_mel[i].idx); }
static void mel_label_update(uint8_t i) {
    lv_label_set_text(g_mel[i].num, mel_name(i, g_mel[i].idx));
    lv_obj_set_style_text_font(g_mel[i].num,
        ui_lang_get() == LANG_CN ? ui_font_melody() : ui_font_text(), 0);
}
static void mel_minus_cb(lv_event_t *e) {
    uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (g_mel[i].idx > 0) g_mel[i].idx--;
    mel_label_update(i);
    mel_set(i);
}
static void mel_plus_cb(lv_event_t *e) {
    uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (g_mel[i].idx < MELODY_MAX_IDX) g_mel[i].idx++;
    mel_label_update(i);
    mel_set(i);
}
static void mel_play_cb(lv_event_t *e) {
    uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    mel_play(i);
}

static void melody_row(lv_obj_t *parent, int x, int y, int w, uint8_t i)
{
    int bs = 36, btn_y = y + 40;
    lv_obj_t *bm = lv_btn_create(parent);
    lv_obj_set_size(bm, bs, bs); lv_obj_set_pos(bm, x, btn_y);
    lv_obj_set_style_radius(bm, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bm, UI_SURFACE_LIGHT, 0);
    lv_obj_set_style_border_width(bm, 0, 0); lv_obj_set_style_shadow_width(bm, 0, 0);
    lv_obj_add_event_cb(bm, mel_minus_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_t *lm = lv_label_create(bm);
    lv_label_set_text(lm, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lm, lv_color_white(), 0);
    lv_obj_center(lm);

    /* Melody name — full width, centered above buttons */
    g_mel[i].num = lv_label_create(parent);
    lv_label_set_text(g_mel[i].num, mel_name(i, g_mel[i].idx));
    lv_obj_set_style_text_font(g_mel[i].num,
        ui_lang_get() == LANG_CN ? ui_font_melody() : ui_font_text(), 0);
    lv_obj_set_style_text_color(g_mel[i].num, UI_TEXT, 0);
    lv_obj_set_style_text_align(g_mel[i].num, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_mel[i].num, w);
    lv_obj_set_pos(g_mel[i].num, x, y - 4);

    /* Stepper +/−/▶ buttons already at btn_y (declared above) */
    lv_obj_t *bp = lv_btn_create(parent);
    lv_obj_set_size(bp, bs, bs); lv_obj_set_pos(bp, x + w/2 - bs + 8, btn_y);
    lv_obj_set_style_radius(bp, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bp, UI_SURFACE_LIGHT, 0);
    lv_obj_set_style_border_width(bp, 0, 0); lv_obj_set_style_shadow_width(bp, 0, 0);
    lv_obj_add_event_cb(bp, mel_plus_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_t *lp = lv_label_create(bp);
    lv_label_set_text(lp, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lp, lv_color_white(), 0);
    lv_obj_center(lp);

    lv_obj_t *by = lv_btn_create(parent);
    lv_obj_set_size(by, bs, bs); lv_obj_set_pos(by, x + w/2 + 8, btn_y);
    lv_obj_set_style_radius(by, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(by, UI_ACCENT, 0);
    lv_obj_set_style_border_width(by, 0, 0); lv_obj_set_style_shadow_width(by, 0, 0);
    lv_obj_add_event_cb(by, mel_play_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    lv_obj_t *ly = lv_label_create(by);
    lv_label_set_text(ly, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(ly, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ly, lv_color_white(), 0);
    lv_obj_center(ly);
}

/* ── Create screen ───────────────────────────────────────────────── */
lv_obj_t *ui_screen_sound_create(lv_obj_t *parent)
{
    int pw = UI_CONTENT_W, ph = UI_CONTENT_H;

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, pw, ph);
    lv_obj_set_pos(root, UI_CONTENT_X, 0);
    lv_obj_set_style_bg_color(root, LG_BG_TOP, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_scroll_dir(root, LV_DIR_NONE);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    int gap = 16, col_w = (pw - 24 - gap * 2) / 3;
    int col_h = 420;  /* full height with melody rows shifted up */
    int x[3] = { 12, 12 + col_w + gap, 12 + (col_w + gap) * 2 };
    const char *titles[3] = {
        ui_lang_str(TXT_KEY_SOUND),
        ui_lang_str(TXT_POWER_SOUND_LABEL),
        ui_lang_str(TXT_ALARM_SOUND)
    };
    bool en[3] = {
        app_controller_get_key_sound(),
        app_controller_get_power_sound(),
        app_controller_get_alarm_sound()
    };
    uint8_t vol[3] = {
        app_controller_get_key_volume(),
        app_controller_get_power_volume(),
        app_controller_get_alarm_volume()
    };
    lv_event_cb_t sw_cbs[3] = { sw_k_cb, sw_p_cb, sw_a_cb };

    g_mel[0] = (mctx_t){ NULL, app_controller_get_key_melody(),       app_controller_set_key_melody,       speaker_click_by_idx };
    g_mel[1] = (mctx_t){ NULL, app_controller_get_power_on_melody(),  app_controller_set_power_on_melody,  speaker_power_on_by_idx };
    g_mel[2] = (mctx_t){ NULL, app_controller_get_power_off_melody(), app_controller_set_power_off_melody, speaker_power_off_by_idx };
    g_mel[3] = (mctx_t){ NULL, app_controller_get_alarm_melody(),     app_controller_set_alarm_melody,     speaker_alarm_preview };

    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = lv_obj_create(root);
        lv_obj_set_size(c, col_w, col_h);
        lv_obj_set_pos(c, x[i], 8);
        ui_flat_group(c);
        lv_obj_set_style_pad_all(c, 0, 0);

        /* Title — left */
        g_title[i] = lv_label_create(c);
        lv_label_set_text(g_title[i], titles[i]);
        lv_obj_set_style_text_font(g_title[i], ui_font_text(), 0);
        lv_obj_set_style_text_color(g_title[i], UI_TEXT_SECONDARY, 0);
        lv_obj_set_pos(g_title[i], 6, 20);

        /* Switch — right, vertically aligned with title */
        g_switch[i] = lv_switch_create(c);
        lv_obj_set_size(g_switch[i], 52, 28);
        lv_obj_set_pos(g_switch[i], col_w - 58, 20);
        lv_obj_set_style_bg_color(g_switch[i], UI_SURFACE_LIGHT, 0);
        lv_obj_set_style_bg_color(g_switch[i], UI_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (en[i]) lv_obj_add_state(g_switch[i], LV_STATE_CHECKED);
        lv_obj_add_event_cb(g_switch[i], sw_cbs[i], LV_EVENT_VALUE_CHANGED, NULL);

        /* Volume % — centered horizontally */
        g_pct[i] = lv_label_create(c);
        lv_label_set_text_fmt(g_pct[i], "%u%%", vol[i]);
        lv_obj_set_style_text_font(g_pct[i], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(g_pct[i], UI_TEXT, 0);
        lv_obj_set_style_text_align(g_pct[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(g_pct[i], col_w - 4);
        lv_obj_set_pos(g_pct[i], 2, 110);

        /* Volume slider — centered with side margins */
        g_slider[i] = lv_slider_create(c);
        lv_obj_set_size(g_slider[i], col_w - 32, 10);
        lv_obj_set_pos(g_slider[i], 16, 190);
        lv_obj_set_style_bg_color(g_slider[i], UI_SURFACE_LIGHT, 0);
        lv_obj_set_style_radius(g_slider[i], 5, 0);
        lv_obj_set_style_bg_color(g_slider[i], LG_ACCENT, LV_PART_INDICATOR);
        lv_obj_set_style_radius(g_slider[i], 5, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_slider[i], UI_ACCENT, LV_PART_KNOB);
        lv_obj_set_style_radius(g_slider[i], LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_width(g_slider[i], 28, LV_PART_KNOB);
        lv_obj_set_style_height(g_slider[i], 28, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(g_slider[i], 6, LV_PART_KNOB);
        lv_obj_set_style_shadow_color(g_slider[i], lv_color_black(), LV_PART_KNOB);
        lv_obj_set_style_shadow_opa(g_slider[i], LV_OPA_30, LV_PART_KNOB);
        lv_slider_set_range(g_slider[i], 0, 100);
        lv_slider_set_value(g_slider[i], vol[i], LV_ANIM_OFF);
        lv_obj_add_event_cb(g_slider[i], vol_cb, LV_EVENT_VALUE_CHANGED, g_pct[i]);
        lv_obj_set_ext_click_area(g_slider[i], 28);
        if (!en[i]) lv_obj_add_state(g_slider[i], LV_STATE_DISABLED);
        if (i > 0) {
            lv_obj_add_event_cb(g_slider[i], vol_cb, LV_EVENT_RELEASED, g_pct[i]);
        }
    }

    /* ── Melody rows — horizontally aligned across all 3 columns ── */
    int mel_y1 = 250, mel_y2 = 340;

    /* Key: one row at mel_y1 */
    melody_row(g_slider[0] ? lv_obj_get_parent(g_slider[0]) : root,
               0, mel_y1, col_w - 8, 0);

    /* Power: two rows at mel_y1 and mel_y2, labels left-aligned with melody name */
    {
        lv_obj_t *pc = g_slider[1] ? lv_obj_get_parent(g_slider[1]) : root;
        lv_obj_t *l1 = lv_label_create(pc);
        lv_label_set_text_fmt(l1, "%s:", ui_lang_str(TXT_SCHED_ON_TIME));
        lv_obj_set_style_text_font(l1, ui_font_text(), 0);
        lv_obj_set_style_text_color(l1, UI_TEXT_MUTED, 0);
        lv_obj_set_pos(l1, 6, mel_y1);
        melody_row(pc, 0, mel_y1, col_w - 8, 1);

        lv_obj_t *l2 = lv_label_create(pc);
        lv_label_set_text_fmt(l2, "%s:", ui_lang_str(TXT_SCHED_OFF_TIME));
        lv_obj_set_style_text_font(l2, ui_font_text(), 0);
        lv_obj_set_style_text_color(l2, UI_TEXT_MUTED, 0);
        lv_obj_set_pos(l2, 6, mel_y2);
        melody_row(pc, 0, mel_y2, col_w - 8, 2);
    }

    /* Alarm: one row at mel_y1 (aligned with Key) */
    melody_row(g_slider[2] ? lv_obj_get_parent(g_slider[2]) : root,
               0, mel_y1, col_w - 8, 3);

    ESP_LOGI(TAG, "sound screen created (%d cols)", 3);
    return root;
}

void ui_screen_sound_lang_update(void)
{
    if (g_title[0]) lv_label_set_text(g_title[0], ui_lang_str(TXT_KEY_SOUND));
    if (g_title[1]) lv_label_set_text(g_title[1], ui_lang_str(TXT_POWER_SOUND_LABEL));
    if (g_title[2]) lv_label_set_text(g_title[2], ui_lang_str(TXT_ALARM_SOUND));
}

void ui_screen_sound_theme_update(void)
{
    for (int i = 0; i < 3; i++) {
        if (g_title[i]) lv_obj_set_style_text_color(g_title[i], UI_TEXT_SECONDARY, 0);
        if (g_pct[i])   lv_obj_set_style_text_color(g_pct[i], UI_TEXT, 0);
        if (g_switch[i]) lv_obj_set_style_bg_color(g_switch[i], UI_SURFACE_LIGHT, 0);
        if (g_slider[i]) lv_obj_set_style_bg_color(g_slider[i], UI_SURFACE_LIGHT, 0);
    }
}

void ui_screen_sound_load_settings(void)
{
    ESP_LOGI(TAG, "sound: key=%s vol=%u pwr=%s vol=%u alarm=%s vol=%u",
             app_controller_get_key_sound()?"ON":"OFF", app_controller_get_key_volume(),
             app_controller_get_power_sound()?"ON":"OFF", app_controller_get_power_volume(),
             app_controller_get_alarm_sound()?"ON":"OFF", app_controller_get_alarm_volume());
}
