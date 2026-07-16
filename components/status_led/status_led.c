#include "status_led.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define WS2812_GPIO       GPIO_NUM_38

static const char *TAG = "status_led";

static rmt_channel_handle_t g_rmt_ch = NULL;
static rmt_encoder_handle_t g_encoder = NULL;
static bool g_on = false;
static uint8_t g_r = 0, g_g = 0, g_b = 0;
static uint8_t g_brightness = 100;  /* 0-100% */
static led_effect_t g_effect = LED_EFFECT_STEADY;
static TimerHandle_t g_effect_timer = NULL;

/* ── WS2812 RMT encoder ──────────────────────────────────────────── */

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
    rmt_symbol_word_t symbols[24];
    size_t sym_count;
} ws2812_encoder_t;

#define RMT_CLK_HZ      10000000  /* 10 MHz (XTAL 40MHz / 4) → 0.1 µs per tick */
#define T0H_TICKS       4         /* 0.4 µs */
#define T0L_TICKS       8         /* 0.8 µs */
#define T1H_TICKS       8         /* 0.8 µs */
#define T1L_TICKS       5         /* 0.5 µs */
#define RESET_TICKS     3000      /* 300 µs reset (>280 µs) */

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *data, size_t data_size, rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *enc = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;

    if (enc->state == 0) {
        const uint8_t *bytes = (const uint8_t *)data;
        enc->sym_count = 0;
        for (int i = 0; i < data_size; i++) {
            uint8_t b = bytes[i];
            for (int bit = 7; bit >= 0; bit--) {
                if (b & (1 << bit)) {
                    enc->symbols[enc->sym_count++] = (rmt_symbol_word_t){
                        .duration0 = T1H_TICKS, .level0 = 1,
                        .duration1 = T1L_TICKS, .level1 = 0,
                    };
                } else {
                    enc->symbols[enc->sym_count++] = (rmt_symbol_word_t){
                        .duration0 = T0H_TICKS, .level0 = 1,
                        .duration1 = T0L_TICKS, .level1 = 0,
                    };
                }
            }
        }
        enc->state = 1;
    }

    if (enc->state == 1) {
        /* Copy pre-built symbols directly to TX buffer — do NOT re-encode */
        enc->copy_encoder->encode(
            enc->copy_encoder, channel, enc->symbols,
            enc->sym_count * sizeof(rmt_symbol_word_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = 2;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return 0;
        }
    }

    if (enc->state == 2) {
        enc->copy_encoder->encode(enc->copy_encoder, channel,
                                  &enc->reset_code, sizeof(enc->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            enc->state = 0;
        }
        *ret_state = session_state;
        return sizeof(enc->reset_code);
    }

    *ret_state = RMT_ENCODING_RESET;
    return 0;
}

static esp_err_t ws2812_encoder_del(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *enc = __containerof(encoder, ws2812_encoder_t, base);
    enc->copy_encoder->del(enc->copy_encoder);
    free(enc);
    return ESP_OK;
}

static esp_err_t ws2812_encoder_reset(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *enc = __containerof(encoder, ws2812_encoder_t, base);
    enc->copy_encoder->reset(enc->copy_encoder);
    enc->state = 0;
    return ESP_OK;
}

static esp_err_t ws2812_new_encoder(rmt_encoder_handle_t *out)
{
    ws2812_encoder_t *enc = calloc(1, sizeof(*enc));
    if (!enc) return ESP_ERR_NO_MEM;

    enc->base.encode = ws2812_encode;
    enc->base.del = ws2812_encoder_del;
    enc->base.reset = ws2812_encoder_reset;

    enc->reset_code = (rmt_symbol_word_t){
        .duration0 = RESET_TICKS, .level0 = 0,
        .duration1 = 0, .level1 = 0,
    };

    rmt_copy_encoder_config_t copy_cfg = {};
    rmt_new_copy_encoder(&copy_cfg, &enc->copy_encoder);

    *out = &enc->base;
    return ESP_OK;
}

static void transmit_grb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_rmt_ch || !g_encoder) {
        ESP_LOGW(TAG, "transmit_grb skipped: ch=%p enc=%p", (void*)g_rmt_ch, (void*)g_encoder);
        return;
    }
    uint8_t grb[3] = {g, r, b};
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    esp_err_t err = rmt_transmit(g_rmt_ch, g_encoder, grb, sizeof(grb), &tx_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit failed: %s (R=%d G=%d B=%d)",
                 esp_err_to_name(err), r, g, b);
    } else {
        ESP_LOGI(TAG, "WS2812 tx: RGB(%d,%d,%d) bri=%d", r, g, b, g_brightness);
    }
}

/* ── Effect timer (50 Hz = 20ms period) ──────────────────────────── */

static void effect_timer_cb(TimerHandle_t timer)
{
    static uint32_t tick = 0;
    tick++;

    if (!g_on) {
        transmit_grb(0, 0, 0);
        return;
    }

    switch (g_effect) {
    case LED_EFFECT_BLINK:
        /* 500ms on, 500ms off → 25 ticks per half-cycle at 50Hz */
        if ((tick / 25) % 2 == 0) {
            uint8_t sr = (uint8_t)((uint16_t)g_r * g_brightness / 100);
            uint8_t sg = (uint8_t)((uint16_t)g_g * g_brightness / 100);
            uint8_t sb = (uint8_t)((uint16_t)g_b * g_brightness / 100);
            transmit_grb(sr, sg, sb);
        } else {
            transmit_grb(0, 0, 0);
        }
        break;
    case LED_EFFECT_FAST_BLINK:
        /* 100ms on, 100ms off */
        if ((tick / 5) % 2 == 0) {
            uint8_t sr = (uint8_t)((uint16_t)g_r * g_brightness / 100);
            uint8_t sg = (uint8_t)((uint16_t)g_g * g_brightness / 100);
            uint8_t sb = (uint8_t)((uint16_t)g_b * g_brightness / 100);
            transmit_grb(sr, sg, sb);
        } else {
            transmit_grb(0, 0, 0);
        }
        break;
    case LED_EFFECT_BREATHE: {
        /* 3000ms full cycle → 150 ticks at 50Hz, triangle wave */
        int phase = tick % 150;
        float wave;
        if (phase < 75) wave = phase / 75.0f;
        else            wave = (150.0f - phase) / 75.0f;
        uint8_t sr = (uint8_t)((uint16_t)g_r * g_brightness / 100 * wave);
        uint8_t sg = (uint8_t)((uint16_t)g_g * g_brightness / 100 * wave);
        uint8_t sb = (uint8_t)((uint16_t)g_b * g_brightness / 100 * wave);
        transmit_grb(sr, sg, sb);
        break;
    }
    case LED_EFFECT_RAINBOW: {
        /* ~5s hue cycle → 240 ticks at 50Hz, 6×40-tick segments */
        uint16_t phase = tick % 240;
        uint8_t r, g, b;
        if (phase < 40) {
            r = 255; g = (uint8_t)(phase * 255 / 40); b = 0;
        } else if (phase < 80) {
            r = (uint8_t)(255 - (phase - 40) * 255 / 40); g = 255; b = 0;
        } else if (phase < 120) {
            r = 0; g = 255; b = (uint8_t)((phase - 80) * 255 / 40);
        } else if (phase < 160) {
            r = 0; g = (uint8_t)(255 - (phase - 120) * 255 / 40); b = 255;
        } else if (phase < 200) {
            r = (uint8_t)((phase - 160) * 255 / 40); g = 0; b = 255;
        } else {
            r = 255; g = 0; b = (uint8_t)(255 - (phase - 200) * 255 / 40);
        }
        /* Apply brightness (0-100%) */
        r = (uint8_t)((uint16_t)r * g_brightness / 100);
        g = (uint8_t)((uint16_t)g * g_brightness / 100);
        b = (uint8_t)((uint16_t)b * g_brightness / 100);
        transmit_grb(r, g, b);
        break;
    }
    default:
        break;
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void status_led_init(void)
{
    /* Already initialized — wifi_prov may call us early and later */
    if (g_rmt_ch && g_encoder) return;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WS2812_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(WS2812_GPIO, 0);

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = WS2812_GPIO,
        .clk_src = RMT_CLK_SRC_XTAL,
        .resolution_hz = RMT_CLK_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .intr_priority = 0,
    };
    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &g_rmt_ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT TX channel fail: %s", esp_err_to_name(err));
        return;
    }

    err = ws2812_new_encoder(&g_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 encoder fail: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_enable(g_rmt_ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT enable fail: %s", esp_err_to_name(err));
        return;
    }

    /* Create effect timer (50Hz), start stopped */
    g_effect_timer = xTimerCreate("led_eff", pdMS_TO_TICKS(20), pdTRUE, NULL, effect_timer_cb);

    status_led_set_on(false);
    ESP_LOGI(TAG, "WS2812 ready on GPIO%d", WS2812_GPIO);
}

void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    g_r = r; g_g = g; g_b = b;
    if (g_on && g_effect == LED_EFFECT_STEADY) {
        uint8_t sr = (uint8_t)((uint16_t)r * g_brightness / 100);
        uint8_t sg = (uint8_t)((uint16_t)g * g_brightness / 100);
        uint8_t sb = (uint8_t)((uint16_t)b * g_brightness / 100);
        transmit_grb(sr, sg, sb);
    }
}

void status_led_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
    g_brightness = pct;
    /* Re-apply to steady output immediately */
    if (g_on && g_effect == LED_EFFECT_STEADY) {
        uint8_t sr = (uint8_t)((uint16_t)g_r * g_brightness / 100);
        uint8_t sg = (uint8_t)((uint16_t)g_g * g_brightness / 100);
        uint8_t sb = (uint8_t)((uint16_t)g_b * g_brightness / 100);
        transmit_grb(sr, sg, sb);
    }
}

void status_led_set_on(bool on)
{
    if (g_on == on) return;
    g_on = on;
    if (!on) {
        transmit_grb(0, 0, 0);
        if (g_effect_timer) xTimerStop(g_effect_timer, 0);
    } else {
        if (g_effect == LED_EFFECT_STEADY) {
            transmit_grb(g_r, g_g, g_b);
        } else if (g_effect_timer) {
            xTimerStart(g_effect_timer, 0);
        }
    }
}

bool status_led_is_on(void) { return g_on; }

void status_led_set_effect(led_effect_t effect)
{
    if (effect == g_effect) return;
    g_effect = effect;

    if (effect == LED_EFFECT_STEADY) {
        if (g_effect_timer) xTimerStop(g_effect_timer, 0);
        if (g_on) transmit_grb(g_r, g_g, g_b);
    } else {
        if (g_effect_timer && g_on) xTimerStart(g_effect_timer, 0);
    }
}
