#include "speaker.h"
#include "board.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "speaker";

static esp_timer_handle_t g_stop_timer;
static esp_timer_handle_t g_melody_timer;

/* melody playback state */
static const uint16_t (*g_melody)[2];
static uint8_t g_melody_len;
static uint8_t g_melody_idx;

/* volume: 0-100, default 70 */
static uint8_t g_volume = 70;
static bool g_click_enabled = true;
static bool g_power_sound_enabled = true;
static uint8_t g_power_volume = 70;
static uint8_t g_prev_volume = 70;
static bool g_is_power_melody = false;
static bool g_is_alarm_melody = false;

/* melody selection indices (0 = original default) */
static uint8_t g_key_melody_idx = 0;
static uint8_t g_power_on_melody_idx = 0;
static uint8_t g_power_off_melody_idx = 0;
static uint8_t g_alarm_melody_idx = 0;

/* ── helpers ─────────────────────────────────────────────────────── */

static void speaker_output(uint16_t freq_hz)
{
    /* Re-attach LEDC to the pin (silent mode disconnected it to kill noise) */
    ledc_set_pin(SPEAKER_PWM_PIN, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
    uint32_t max_duty = (1U << SPEAKER_PWM_RES) - 1;
    uint32_t duty = ((uint32_t)g_volume * max_duty) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    gpio_set_level(SPEAKER_SHDN_PIN, 1);   /* enable PAM8302 after PWM stable */
}

static void speaker_silent(void)
{
    gpio_set_level(SPEAKER_SHDN_PIN, 0);   /* disable PAM8302 */
    /* Detach LEDC from the pin and force GPIO LOW — kills PWM leakage
     * through the RC filter that the amp can still hear */
    gpio_set_direction(SPEAKER_PWM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SPEAKER_PWM_PIN, 0);
}

/* ── timer callbacks ──────────────────────────────────────────────── */

static void stop_timer_cb(void *arg)
{
    speaker_silent();
}

static void melody_timer_cb(void *arg)
{
    if (g_melody_idx >= g_melody_len) {
        speaker_silent();
        if (g_is_power_melody || g_is_alarm_melody) {
            g_volume = g_prev_volume;
            g_is_power_melody = false;
            g_is_alarm_melody = false;
        }
        return;
    }
    uint16_t freq = g_melody[g_melody_idx][0];
    uint16_t dur  = g_melody[g_melody_idx][1];
    g_melody_idx++;

    if (freq == 0) {
        /* rest — silent for duration */
        speaker_silent();
    } else {
        speaker_output(freq);
    }
    /* schedule next note or stop */
    if (g_melody_idx < g_melody_len) {
        esp_timer_stop(g_melody_timer);
        esp_err_t e = esp_timer_start_once(g_melody_timer, dur * 1000ULL);
        if (e != ESP_OK) { ESP_LOGW(TAG, "melody timer: %s", esp_err_to_name(e)); speaker_silent(); return; }
    } else {
        esp_timer_stop(g_stop_timer);
        esp_err_t e = esp_timer_start_once(g_stop_timer, dur * 1000ULL);
        if (e != ESP_OK) { ESP_LOGW(TAG, "stop timer: %s", esp_err_to_name(e)); speaker_silent(); return; }
    }
}

/* ── init ─────────────────────────────────────────────────────────── */

void speaker_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SPEAKER_PWM_RES,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = SPEAKER_PWM_FREQ,  /* carrier — inaudible at idle */
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) { ESP_LOGE(TAG, "LEDC timer fail: %s", esp_err_to_name(err)); return; }

    ledc_channel_config_t ch = {
        .gpio_num   = SPEAKER_PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
    };
    err = ledc_channel_config(&ch);
    if (err != ESP_OK) { ESP_LOGE(TAG, "LEDC channel fail: %s", esp_err_to_name(err)); return; }

    /* SHDN pin: LOW = amp off, HIGH = amp on */
    gpio_set_direction(SPEAKER_SHDN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SPEAKER_SHDN_PIN, 0);

    /* create one-shot timers */
    esp_timer_create_args_t stop_args = {
        .callback = stop_timer_cb,
        .name     = "spk_stop",
    };
    err = esp_timer_create(&stop_args, &g_stop_timer);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Timer create fail: %s", esp_err_to_name(err)); return; }

    esp_timer_create_args_t mel_args = {
        .callback = melody_timer_cb,
        .name     = "spk_melody",
    };
    err = esp_timer_create(&mel_args, &g_melody_timer);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Timer create fail: %s", esp_err_to_name(err)); return; }

    ESP_LOGI(TAG, "ready (GPIO%d, %dHz PWM carrier)", SPEAKER_PWM_PIN, SPEAKER_PWM_FREQ);
}

/* ── volume control ───────────────────────────────────────────────── */

void speaker_set_volume(uint8_t pct)
{
    if (pct > 90) pct = 90;  /* 90% max — avoids class-D clipping noise */
    if (pct < 1) pct = 1;
    g_volume = pct;
}

uint8_t speaker_get_volume(void) { return g_volume; }

void speaker_set_click_enabled(bool en) { g_click_enabled = en; }
bool speaker_is_click_enabled(void) { return g_click_enabled; }

void speaker_set_power_sound_enabled(bool en) { g_power_sound_enabled = en; }
bool speaker_is_power_sound_enabled(void) { return g_power_sound_enabled; }

void speaker_set_power_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    g_power_volume = pct;
}
uint8_t speaker_get_power_volume(void) { return g_power_volume; }

/* alarm volume & enable (independent from key/power) */
static uint8_t g_alarm_volume = 70;
static bool g_alarm_sound_enabled = true;

void speaker_set_alarm_sound_enabled(bool en) { g_alarm_sound_enabled = en; }
bool speaker_is_alarm_sound_enabled(void) { return g_alarm_sound_enabled; }

void speaker_set_alarm_volume(uint8_t pct)
{
    if (pct > 100) pct = 100;
    g_alarm_volume = pct;
}
uint8_t speaker_get_alarm_volume(void) { return g_alarm_volume; }

void speaker_set_key_melody(uint8_t idx)       { if (idx <= MELODY_KEY_CLICK_MAX)  g_key_melody_idx = idx; }
uint8_t speaker_get_key_melody(void)           { return g_key_melody_idx; }
void speaker_set_power_on_melody(uint8_t idx)  { if (idx <= MELODY_POWER_ON_MAX)   g_power_on_melody_idx = idx; }
uint8_t speaker_get_power_on_melody(void)      { return g_power_on_melody_idx; }
void speaker_set_power_off_melody(uint8_t idx) { if (idx <= MELODY_POWER_OFF_MAX)  g_power_off_melody_idx = idx; }
uint8_t speaker_get_power_off_melody(void)     { return g_power_off_melody_idx; }

/* ── public API ───────────────────────────────────────────────────── */

void speaker_tone(uint16_t freq_hz, uint16_t duration_ms)
{
    speaker_output(freq_hz);
    esp_timer_stop(g_stop_timer);
    ESP_ERROR_CHECK(esp_timer_start_once(g_stop_timer, duration_ms * 1000ULL));
}

void speaker_stop(void)
{
    esp_timer_stop(g_stop_timer);
    esp_timer_stop(g_melody_timer);
    speaker_silent();
}

/* ── pre-defined sounds ───────────────────────────────────────────── */

/* ── Key-click melodies (idx 0-3) ─────────────────────────────────── */

/* 0: Mario coin — B6→E7 blip (original) */
static const uint16_t click0_melody[][2] = {
    {1976, 40}, {0, 15}, {2637, 70},
};

/* 1: Short tick — single 1kHz click, 40ms */
static const uint16_t click1_melody[][2] = {
    {1000, 40},
};

/* 2: Water drop — quick descending glissando */
static const uint16_t click2_melody[][2] = {
    {880, 40}, {0, 10}, {770, 40}, {0, 10}, {660, 50},
};

/* 3: Electronic beep — 2kHz short burst */
static const uint16_t click3_melody[][2] = {
    {2000, 50},
};

/* 4: Soft tick — very subtle low click */
static const uint16_t click4_melody[][2] = {
    {800, 30},
};

/* 5: Double click — two quick pulses */
static const uint16_t click5_melody[][2] = {
    {1200, 35}, {0, 30}, {1200, 35},
};

static const struct {
    const uint16_t (*data)[2];
    uint8_t len;
} g_key_melodies[6] = {
    { click0_melody, sizeof(click0_melody) / sizeof(click0_melody[0]) },
    { click1_melody, sizeof(click1_melody) / sizeof(click1_melody[0]) },
    { click2_melody, sizeof(click2_melody) / sizeof(click2_melody[0]) },
    { click3_melody, sizeof(click3_melody) / sizeof(click3_melody[0]) },
    { click4_melody, sizeof(click4_melody) / sizeof(click4_melody[0]) },
    { click5_melody, sizeof(click5_melody) / sizeof(click5_melody[0]) },
};

/* ── Power-on melodies (idx 0-3) ──────────────────────────────────── */

/* 0: Super Mario Bros Main Theme (original)
 * RTTTL: SuperMario:d=4,o=5,b=140:16e6,16e6,32p,16e6,32p,... */
static const uint16_t poweron0_melody[][2] = {
    {1319, 107}, {1319, 107}, {0, 54}, {1319, 107}, {0, 54},
    {1047, 107}, {1319, 107}, {0, 54},
    {1568, 107}, {0, 54}, {0, 54}, {0, 54},
    {784, 107}, {0, 54}, {0, 54}, {0, 54},
    {1319, 107}, {1175, 107}, {0, 54},
    {1047, 107}, {1047, 107}, {0, 54},
    {784, 107},
    {1319, 107}, {1760, 107}, {1976, 107}, {1760, 107}, {1760, 107},
    {1568, 107}, {1319, 107}, {1568, 107}, {1760, 107},
    {1397, 107}, {1568, 107}, {1319, 107},
    {1047, 107}, {1175, 107}, {988, 200},
};

/* 1: Ascending major scale — C5→C6 */
static const uint16_t poweron1_melody[][2] = {
    {523, 100}, {587, 100}, {659, 100}, {698, 100},
    {784, 100}, {880, 100}, {988, 100}, {1047, 150},
};

/* 2: Ding-dong doorbell chime */
static const uint16_t poweron2_melody[][2] = {
    {800, 200}, {0, 60}, {1000, 350},
};

/* 3: C-E-G-C major arpeggio — triumphant */
static const uint16_t poweron3_melody[][2] = {
    {523, 120}, {659, 120}, {784, 120}, {1047, 200},
};

/* 4: Twinkle Twinkle Little Star opening */
static const uint16_t poweron4_melody[][2] = {
    {523, 150}, {523, 150}, {784, 150}, {784, 150},
    {880, 150}, {880, 150}, {784, 300},
    {698, 150}, {698, 150}, {659, 150}, {659, 150},
    {587, 150}, {587, 150}, {523, 300},
};

/* 5: Short fanfare — C-E-G-C-E-G ascending */
static const uint16_t poweron5_melody[][2] = {
    {523, 80}, {659, 80}, {784, 80}, {1047, 120},
    {0, 40}, {1319, 120}, {1568, 250},
};

static const struct {
    const uint16_t (*data)[2];
    uint8_t len;
} g_power_on_melodies[6] = {
    { poweron0_melody, sizeof(poweron0_melody) / sizeof(poweron0_melody[0]) },
    { poweron1_melody, sizeof(poweron1_melody) / sizeof(poweron1_melody[0]) },
    { poweron2_melody, sizeof(poweron2_melody) / sizeof(poweron2_melody[0]) },
    { poweron3_melody, sizeof(poweron3_melody) / sizeof(poweron3_melody[0]) },
    { poweron4_melody, sizeof(poweron4_melody) / sizeof(poweron4_melody[0]) },
    { poweron5_melody, sizeof(poweron5_melody) / sizeof(poweron5_melody[0]) },
};

/* ── Power-off melodies (idx 0-3) ─────────────────────────────────── */

/* 0: Descending G5→E5→C5 (original) */
static const uint16_t poweroff0_melody[][2] = {
    {784, 120}, {0, 40}, {659, 120}, {0, 40}, {523, 200},
};

/* 1: Long descending scale — C6→C5 */
static const uint16_t poweroff1_melody[][2] = {
    {1047, 100}, {988, 100}, {880, 100}, {784, 100},
    {698, 100}, {659, 100}, {587, 100}, {523, 200},
};

/* 2: Ding-dong descending */
static const uint16_t poweroff2_melody[][2] = {
    {1000, 200}, {0, 60}, {800, 350},
};

/* 3: Descending minor arpeggio — A-F-D-A */
static const uint16_t poweroff3_melody[][2] = {
    {880, 150}, {698, 150}, {587, 150}, {440, 300},
};

/* 4: Bye-bye two-note chime */
static const uint16_t poweroff4_melody[][2] = {
    {784, 200}, {0, 100}, {523, 400},
};

/* 5: Gentle wind-down — slowing descending notes */
static const uint16_t poweroff5_melody[][2] = {
    {1047, 80}, {988, 100}, {880, 120}, {784, 150},
    {698, 180}, {587, 220}, {523, 300},
};

static const struct {
    const uint16_t (*data)[2];
    uint8_t len;
} g_power_off_melodies[6] = {
    { poweroff0_melody, sizeof(poweroff0_melody) / sizeof(poweroff0_melody[0]) },
    { poweroff1_melody, sizeof(poweroff1_melody) / sizeof(poweroff1_melody[0]) },
    { poweroff2_melody, sizeof(poweroff2_melody) / sizeof(poweroff2_melody[0]) },
    { poweroff3_melody, sizeof(poweroff3_melody) / sizeof(poweroff3_melody[0]) },
    { poweroff4_melody, sizeof(poweroff4_melody) / sizeof(poweroff4_melody[0]) },
    { poweroff5_melody, sizeof(poweroff5_melody) / sizeof(poweroff5_melody[0]) },
};

/* ── Alarm / Wi-Fi prov (unchanged, no selection needed) ───────────── */

/* ── Alarm melodies (6 selectable) ───────────────────────────────── */
/* 0: Classic alternating siren */
static const uint16_t alarm0_melody[][2] = {
    {1000,200},{0,100},{500,200},{0,100},{1000,200},{0,100},{500,200},{0,100},{1000,200},{0,100},{500,200},
};
/* 1: Rapid beep — urgent */
static const uint16_t alarm1_melody[][2] = {
    {800,120},{0,60},{800,120},{0,60},{800,120},{0,60},{800,120},{0,60},{800,120},
};
/* 2: High-low sweep */
static const uint16_t alarm2_melody[][2] = {
    {400,150},{600,150},{800,150},{1000,150},{1200,150},{0,100},
    {1200,150},{1000,150},{800,150},{600,150},{400,150},
};
/* 3: Triple pulse */
static const uint16_t alarm3_melody[][2] = {
    {880,100},{0,50},{880,100},{0,50},{880,100},{0,200},
    {660,100},{0,50},{660,100},{0,50},{660,100},{0,200},
    {880,100},{0,50},{880,100},{0,50},{880,100},
};
/* 4: Continuous fast tick */
static const uint16_t alarm4_melody[][2] = {
    {1500,80},{0,40},{1500,80},{0,40},{1500,80},{0,40},{1500,80},{0,40},
    {1500,80},{0,40},{1500,80},{0,40},{1500,80},{0,40},{1500,80},
};
/* 5: Low rumble — for critical */
static const uint16_t alarm5_melody[][2] = {
    {440,300},{0,50},{440,300},{0,50},{440,300},{0,50},{440,300},
};

static struct { const uint16_t (*data)[2]; uint8_t len; } g_alarm_melodies[] = {
    { alarm0_melody, sizeof(alarm0_melody)/sizeof(alarm0_melody[0]) },
    { alarm1_melody, sizeof(alarm1_melody)/sizeof(alarm1_melody[0]) },
    { alarm2_melody, sizeof(alarm2_melody)/sizeof(alarm2_melody[0]) },
    { alarm3_melody, sizeof(alarm3_melody)/sizeof(alarm3_melody[0]) },
    { alarm4_melody, sizeof(alarm4_melody)/sizeof(alarm4_melody[0]) },
    { alarm5_melody, sizeof(alarm5_melody)/sizeof(alarm5_melody[0]) },
};

static const uint16_t wifi_prov_melody[][2] = {
    {1200, 80}, {0, 200}, {1200, 80},
};

/* ── Play / preview functions ─────────────────────────────────────── */

static void play_melody_from(const uint16_t (*m)[2], uint8_t len,
                             bool is_power)
{
    if (is_power) {
        g_is_power_melody = true;
        g_prev_volume = g_volume;
        g_volume = g_power_volume;
    }
    g_melody = m;
    g_melody_len = len;
    g_melody_idx = 0;
    melody_timer_cb(NULL);
}

void speaker_click(void)
{
    ESP_LOGI(TAG, "click: enabled=%d volume=%u idx=%u",
             g_click_enabled, g_volume, g_key_melody_idx);
    if (!g_click_enabled) return;
    play_melody_from(g_key_melodies[g_key_melody_idx].data,
                     g_key_melodies[g_key_melody_idx].len, false);
}

void speaker_click_by_idx(uint8_t idx)
{
    if (idx > MELODY_KEY_CLICK_MAX) idx = 0;
    play_melody_from(g_key_melodies[idx].data,
                     g_key_melodies[idx].len, false);
}

void speaker_power_on(void)
{
    if (!g_power_sound_enabled) return;
    play_melody_from(g_power_on_melodies[g_power_on_melody_idx].data,
                     g_power_on_melodies[g_power_on_melody_idx].len, true);
}

void speaker_power_on_by_idx(uint8_t idx)
{
    if (idx > MELODY_POWER_ON_MAX) idx = 0;
    play_melody_from(g_power_on_melodies[idx].data,
                     g_power_on_melodies[idx].len, true);
}

void speaker_power_off(void)
{
    if (!g_power_sound_enabled) return;
    play_melody_from(g_power_off_melodies[g_power_off_melody_idx].data,
                     g_power_off_melodies[g_power_off_melody_idx].len, true);
}

void speaker_power_off_by_idx(uint8_t idx)
{
    if (idx > MELODY_POWER_OFF_MAX) idx = 0;
    play_melody_from(g_power_off_melodies[idx].data,
                     g_power_off_melodies[idx].len, true);
}

void speaker_alarm(void) {
    if (!g_alarm_sound_enabled) return;
    if (g_alarm_melody_idx > MELODY_ALARM_MAX) g_alarm_melody_idx = 0;
    g_is_alarm_melody = true;
    g_prev_volume = g_volume;
    g_volume = g_alarm_volume;
    g_melody = g_alarm_melodies[g_alarm_melody_idx].data;
    g_melody_len = g_alarm_melodies[g_alarm_melody_idx].len;
    g_melody_idx = 0;
    melody_timer_cb(NULL);
}

void speaker_set_alarm_melody(uint8_t idx) { if(idx<=MELODY_ALARM_MAX) g_alarm_melody_idx=idx; }
uint8_t speaker_get_alarm_melody(void) { return g_alarm_melody_idx; }

void speaker_alarm_preview(uint8_t idx) {
    if(idx > MELODY_ALARM_MAX) return;
    g_is_alarm_melody = true;
    g_prev_volume = g_volume;
    g_volume = g_alarm_volume;
    g_melody = g_alarm_melodies[idx].data;
    g_melody_len = g_alarm_melodies[idx].len;
    g_melody_idx = 0;
    melody_timer_cb(NULL);
}

void speaker_wifi_prov(void) {
    g_melody = wifi_prov_melody;
    g_melody_len = sizeof(wifi_prov_melody) / sizeof(wifi_prov_melody[0]);
    g_melody_idx = 0;
    melody_timer_cb(NULL);
}
