#pragma once

#include <stdint.h>

/* ── Volume control ──────────────────────────────────────────────── */

/** Set volume 0-100 (scales PWM duty). Stored in RAM only — caller must persist. */
void speaker_set_volume(uint8_t pct);

/** Get current volume 0-100. */
uint8_t speaker_get_volume(void);

/** Enable/disable key click sounds (persisted elsewhere). */
void speaker_set_click_enabled(bool en);
bool speaker_is_click_enabled(void);

/** Enable/disable power-on/off melodies (persisted elsewhere). */
void speaker_set_power_sound_enabled(bool en);
bool speaker_is_power_sound_enabled(void);

/** Set power-on/off melody volume 0-100 (separate from key volume). */
void speaker_set_power_volume(uint8_t pct);
uint8_t speaker_get_power_volume(void);

/** Enable/disable alarm sounds. */
void speaker_set_alarm_sound_enabled(bool en);
bool speaker_is_alarm_sound_enabled(void);

/** Set alarm melody volume 0-100 (separate from key/power volume). */
void speaker_set_alarm_volume(uint8_t pct);
uint8_t speaker_get_alarm_volume(void);

/* ── Public API ──────────────────────────────────────────────────── */

/** One-time init: configure LEDC PWM at SPEAKER_PWM_PIN, output silent. */
void speaker_init(void);

/** Play a single tone: freq_hz (e.g. 1000), duration_ms (e.g. 50).
 *  Non-blocking — returns immediately; tone stops after duration_ms via timer. */
void speaker_tone(uint16_t freq_hz, uint16_t duration_ms);

/** Stop any playing tone immediately. */
void speaker_stop(void);

/* ── Melody selection indices ─────────────────────────────────────── */

#define MELODY_KEY_CLICK_MAX  5   /* 0-5: Mario coin, tick, water, beep, soft tick, double click */
#define MELODY_POWER_ON_MAX   5   /* 0-5: Mario theme, scale, ding-dong, arpeggio, twinkle, fanfare */
#define MELODY_POWER_OFF_MAX  5
#define MELODY_ALARM_MAX      5

void speaker_set_key_melody(uint8_t idx);
uint8_t speaker_get_key_melody(void);
void speaker_set_power_on_melody(uint8_t idx);
uint8_t speaker_get_power_on_melody(void);
void speaker_set_alarm_melody(uint8_t idx);
uint8_t speaker_get_alarm_melody(void);
void speaker_set_power_off_melody(uint8_t idx);
uint8_t speaker_get_power_off_melody(void);

/** Play a specific melody by index (for web preview — does NOT persist). */
void speaker_click_by_idx(uint8_t idx);
void speaker_power_on_by_idx(uint8_t idx);
void speaker_power_off_by_idx(uint8_t idx);

/* ── Pre-defined sound effects ───────────────────────────────────── */

/** Short click for button/touch feedback — selected via key_melody index. */
void speaker_click(void);

/** Power-on melody — selected via power_on_melody index. */
void speaker_power_on(void);

/** Power-off melody — selected via power_off_melody index. */
void speaker_power_off(void);

/** Air-quality alarm — plays selected melody */
void speaker_alarm(void);
/** Preview alarm melody by index (0–MELODY_ALARM_MAX) */
void speaker_alarm_preview(uint8_t idx);

/** WiFi provisioning mode: 2 quick beeps */
void speaker_wifi_prov(void);
