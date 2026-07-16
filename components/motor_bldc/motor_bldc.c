#include "motor_bldc.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

static const char *TAG = "motor";

static motor_state_t g_state = MOTOR_STATE_STOPPED;
static uint8_t g_target_speed = 0;
static uint8_t g_current_speed = 0;
static uint16_t g_rpm = 0;
static motor_state_cb_t g_callback = NULL;
static void *g_cb_user_data = NULL;

static pcnt_unit_handle_t g_pcnt_unit = NULL;
static TimerHandle_t g_rpm_timer = NULL;
static TimerHandle_t g_ramp_timer = NULL;
static uint8_t g_ramp_target_duty = 0;
static uint8_t g_ramp_kick_duty = 0;
static int g_ramp_step = 0;
static int g_ramp_total = 30;
#define RAMP_STEPS_START  10    /* startup ramp: 10×50ms = 0.5s */
#define RAMP_STEPS_SPEED  16    /* speed-change ramp: 16×50ms = 0.8s */
#define RAMP_INTERVAL_MS 50

/* MCPWM: fixed 1kHz CLK, variable duty cycle for speed control */
static mcpwm_timer_handle_t g_mcpwm_timer = NULL;
static mcpwm_oper_handle_t g_mcpwm_oper = NULL;
static mcpwm_cmpr_handle_t g_mcpwm_cmpr = NULL;
static mcpwm_gen_handle_t g_clk_gen = NULL;

#define MCPWM_RESOLUTION_HZ 1000000  /* 1 MHz → 1 µs per tick */
#define MOTOR_CLK_FREQ_HZ   1000     /* fixed CLK frequency */
#define MOTOR_CLK_PERIOD    (MCPWM_RESOLUTION_HZ / MOTOR_CLK_FREQ_HZ)  /* 1000 ticks */

#define RPM_INTERVAL_S      1        /* RPM polling interval in seconds */

/* CLK is active-LOW: LOW = motor runs, HIGH = stop.
   Speed 1-100% maps to HIGH duty 90-10% (i.e. LOW time 10-90%).
   100% speed = 90% of physical max (10% HIGH duty, not 0%).
   MOTOR_MIN_SPEED_DUTY = 90% HIGH duty for 1% speed.
   MOTOR_MAX_SPEED_DUTY = 10% HIGH duty for 100% speed. */
#define MOTOR_MIN_SPEED_DUTY 90
#define MOTOR_MAX_SPEED_DUTY 10

static void update_state(motor_state_t new_state)
{
    if (g_state != new_state) {
        g_state = new_state;
        if (g_callback) g_callback(g_state, g_rpm, g_cb_user_data);
    }
}

/* Set CLK active-HIGH duty cycle. pct = HIGH portion of the PWM.
   Low pct = mostly LOW = motor runs fast. */
static void mcpwm_set_duty(uint8_t pct)
{
    if (pct > 100) pct = 100;

    /* Active-high PWM — HIGH from counter=0 until compare match.
       duty% = (period - cmp) / period * 100  →  cmp = period * (100 - duty) / 100 */
    uint32_t cmp = MOTOR_CLK_PERIOD * (100 - pct) / 100;
    if (cmp < 1) cmp = 1;
    if (cmp >= MOTOR_CLK_PERIOD) cmp = MOTOR_CLK_PERIOD - 1;

    mcpwm_comparator_set_compare_value(g_mcpwm_cmpr, cmp);
}

static void rpm_timer_cb(TimerHandle_t xTimer)
{
    if (!g_pcnt_unit) return;
    int count = 0;
    if (pcnt_unit_get_count(g_pcnt_unit, &count) != ESP_OK) return;
    pcnt_unit_clear_count(g_pcnt_unit);
    g_rpm = (uint16_t)((count * 60) / (MOTOR_PULSES_PER_REV * RPM_INTERVAL_S));
    /* Only log when RPM changes significantly — avoids ~1/sec spam */
    static uint16_t last_rpm;
    static uint8_t rpm_log_skip;
    if (++rpm_log_skip >= 60 || (g_rpm > last_rpm + 50) || (g_rpm + 50 < last_rpm)) {
        ESP_LOGI(TAG, "RPM=%u", g_rpm);
        last_rpm = g_rpm; rpm_log_skip = 0;
    }
}

static void ramp_timer_cb(TimerHandle_t xTimer);

static void ramp_timer_cb(TimerHandle_t xTimer)
{
    g_ramp_step++;
    if (g_ramp_step >= g_ramp_total) {
        mcpwm_set_duty(g_ramp_target_duty);
        g_current_speed = g_target_speed;
        update_state(MOTOR_STATE_RUNNING);
        ESP_LOGI(TAG, "Ramp done, speed=%d%% duty=%d%%",
                 g_current_speed, g_ramp_target_duty);
        xTimerStop(g_ramp_timer, 0);
        return;
    }
    int d = (int)g_ramp_kick_duty +
        ((int)(g_ramp_target_duty - g_ramp_kick_duty) * g_ramp_step) / g_ramp_total;
    if (d < 1) d = 1;
    mcpwm_set_duty((uint8_t)d);
}

void motor_init(void)
{
    ESP_LOGI(TAG, "Initializing 43F704S100 BLDC motor (1kHz PWM duty-cycle)");

    /* ── MCPWM timer: 1 MHz, fixed 1kHz period ── */
    mcpwm_timer_config_t timer_cfg = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = MCPWM_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = MOTOR_CLK_PERIOD,
        .intr_priority = 0,
        .flags = { .update_period_on_empty = false },
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &g_mcpwm_timer));

    /* ── MCPWM operator ── */
    mcpwm_operator_config_t oper_cfg = {
        .group_id = 0,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &g_mcpwm_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(g_mcpwm_oper, g_mcpwm_timer));

    /* ── MCPWM comparator ── */
    mcpwm_comparator_config_t cmpr_cfg = {
        .intr_priority = 0,
        .flags = { .update_cmp_on_tez = false },
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(g_mcpwm_oper, &cmpr_cfg, &g_mcpwm_cmpr));

    /* ── CLK generator: GPIO39 ── */
    mcpwm_generator_config_t clk_gen_cfg = {
        .gen_gpio_num = MOTOR_CLK_PIN,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(g_mcpwm_oper, &clk_gen_cfg, &g_clk_gen));

    ESP_ERROR_CHECK(mcpwm_timer_enable(g_mcpwm_timer));

    /* ── FG pulse counter ── */
    pcnt_unit_config_t pcnt_cfg = {
        .low_limit = -32767,
        .high_limit = 32767,
        .intr_priority = 0,
        .flags = { .accum_count = 1 },
    };
    pcnt_new_unit(&pcnt_cfg, &g_pcnt_unit);

    pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 10000 };
    pcnt_unit_set_glitch_filter(g_pcnt_unit, &filter);

    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num = MOTOR_FG_PIN,
        .level_gpio_num = -1,
        .flags = { .invert_edge_input = false, .invert_level_input = false },
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    pcnt_new_channel(g_pcnt_unit, &chan_cfg, &pcnt_chan);
    pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                 PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                  PCNT_CHANNEL_LEVEL_ACTION_KEEP);

    gpio_set_pull_mode(MOTOR_FG_PIN, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "FG pin GPIO%u pull-up enabled", (unsigned)MOTOR_FG_PIN);

    pcnt_unit_enable(g_pcnt_unit);
    pcnt_unit_clear_count(g_pcnt_unit);
    pcnt_unit_start(g_pcnt_unit);

    g_rpm_timer = xTimerCreate("motor_rpm", pdMS_TO_TICKS(RPM_INTERVAL_S * 1000),
                               pdTRUE, NULL, rpm_timer_cb);
    if (g_rpm_timer) xTimerStart(g_rpm_timer, 0);

    g_ramp_timer = xTimerCreate("motor_ramp", pdMS_TO_TICKS(RAMP_INTERVAL_MS),
                                pdTRUE, NULL, ramp_timer_cb);

    ESP_LOGI(TAG, "Motor initialized, CLK=%d Hz, speed→duty 1%%→%d%% 100%%→%d%%",
             MOTOR_CLK_FREQ_HZ, MOTOR_MIN_SPEED_DUTY, MOTOR_MAX_SPEED_DUTY);
}

/* Speed 1-100% → HIGH duty 90-10% (inverted: more LOW time = faster)
   100% speed = 90% of physical max (10% HIGH duty) */
static uint8_t speed_to_duty(uint8_t pct)
{
    if (pct == 0) return 100;   /* 100% HIGH = motor stopped */
    /* Linear map: 1%→85%, 100%→5% */
    return MOTOR_MIN_SPEED_DUTY - ((uint32_t)(pct - 1) *
            (MOTOR_MIN_SPEED_DUTY - MOTOR_MAX_SPEED_DUTY)) / 99;
}

void motor_start(void)
{
    ESP_LOGI(TAG, "motor_start: state=%d target_speed=%d", g_state, g_target_speed);
    if (g_state == MOTOR_STATE_RUNNING) return;

    bool was_stopped = (g_state == MOTOR_STATE_STOPPED);
    update_state(MOTOR_STATE_RAMPING_UP);

    uint8_t target_duty = speed_to_duty(g_target_speed);

    if (!was_stopped) {
        mcpwm_timer_start_stop(g_mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
    }

    /* Set up CLK generator actions: HIGH on EMPTY, LOW on COMPARE → active-high PWM */
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(g_clk_gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(g_clk_gen,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, g_mcpwm_cmpr, MCPWM_GEN_ACTION_LOW)));

    /* Kick-start at minimum HIGH duty (maximum LOW time) for strongest torque,
       then ramp to target HIGH duty. Lower HIGH duty = faster motor. */
    uint8_t kick_duty = MOTOR_MAX_SPEED_DUTY;   /* 5% HIGH = 95% LOW = max torque */
    if (target_duty < kick_duty) kick_duty = target_duty;  /* even faster if needed */

    mcpwm_set_duty(kick_duty);
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(g_mcpwm_timer, MCPWM_TIMER_START_NO_STOP));

    g_ramp_target_duty = target_duty;
    g_ramp_kick_duty = kick_duty;
    g_ramp_total = RAMP_STEPS_START;
    g_ramp_step = 0;
    xTimerStart(g_ramp_timer, 0);

    ESP_LOGI(TAG, "Start: kick=%d%% target=%d%% (speed=%d%%)",
             kick_duty, target_duty, g_target_speed);
}

void motor_stop(void)
{
    if (g_state == MOTOR_STATE_STOPPED) return;
    xTimerStop(g_ramp_timer, 0);
    update_state(MOTOR_STATE_RAMPING_DOWN);

    /* Ramp duty toward 100% (all HIGH = motor off) */
    uint8_t cur = speed_to_duty(g_current_speed);
    if (cur > 90) cur = 90;
    int steps = 10;
    for (int i = 0; i <= steps; i++) {
        int d = cur + ((100 - cur) * i) / steps;
        if (d > 100) d = 100;
        mcpwm_set_duty((uint8_t)d);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    mcpwm_timer_start_stop(g_mcpwm_timer, MCPWM_TIMER_STOP_EMPTY);
    g_current_speed = 0;
    g_rpm = 0;
    update_state(MOTOR_STATE_STOPPED);
    ESP_LOGI(TAG, "Motor stopped");
}

void motor_set_speed(uint8_t pct)
{
    if (pct > 100) pct = 100;
    if (pct > 0 && pct < 8) pct = 8;
    g_target_speed = pct;

    uint8_t duty = speed_to_duty(pct);

    if (g_state == MOTOR_STATE_STOPPED) {
        return;
    }

    /* Ramp from current duty to target duty */
    uint8_t cur_duty = speed_to_duty(g_current_speed);
    if (cur_duty == 0) cur_duty = duty;
    g_ramp_kick_duty = cur_duty;
    g_ramp_target_duty = duty;
    g_ramp_total = RAMP_STEPS_SPEED;
    g_ramp_step = 0;

    xTimerStop(g_ramp_timer, 0);
    if (g_state != MOTOR_STATE_RAMPING_UP) {
        update_state(MOTOR_STATE_RAMPING_UP);
    }
    xTimerStart(g_ramp_timer, 0);

    ESP_LOGI(TAG, "Speed=%d%% → duty=%d%% (from %d%%, ramp=%d steps, brake=%s)",
             pct, duty, cur_duty, RAMP_STEPS_SPEED,
             duty > cur_duty + 25 ? "YES" : "no");
}

uint8_t motor_get_speed(void) { return g_current_speed; }

uint16_t motor_get_rpm(void) { return g_rpm; }

motor_state_t motor_get_state(void) { return g_state; }

void motor_set_state_callback(motor_state_cb_t cb, void *user_data)
{
    g_callback = cb;
    g_cb_user_data = user_data;
}
