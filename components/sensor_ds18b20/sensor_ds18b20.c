#include "sensor_ds18b20.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "rom/ets_sys.h"
#include <string.h>

static const char *TAG = "ds18b20";

#define DS18B20_NVS_NS  "ds18b20"
#define NVS_KEY_OFFSET  "cal_ofs"
#define NVS_KEY_CALIB   "calib"

/* ── 1-Wire GPIO bit-bang ─────────────────────────────────────────── */

/* Standard-speed 1-Wire timing (µs) */
#define OW_RESET_LOW_US   490
#define OW_RESET_WAIT_US   70
#define OW_PRESENCE_WAIT  240
#define OW_WRITE_1_LOW_US   2
#define OW_WRITE_0_LOW_US  62
#define OW_READ_LOW_US      2
#define OW_READ_SAMPLE_US   8
#define OW_RECOVERY_US      3

/* DS18B20 ROM / function commands */
#define DS18B20_CMD_CONVERT_T     0x44
#define DS18B20_CMD_READ_SCRATCH  0xBE
#define DS18B20_CMD_WRITE_SCRATCH 0x4E
#define DS18B20_CMD_COPY_SCRATCH  0x48
#define DS18B20_CMD_SKIP_ROM      0xCC

#define DS18B20_RESOLUTION        12
#define DS18B20_CONVERT_MS       750   /* max conversion time @ 12-bit */

static int g_ow_pin = -1;
static ds18b20_data_cb_t g_callback = NULL;
static void *g_cb_user_data = NULL;
static float g_cal_offset = 0.0f;
static bool g_calibrated = false;
static uint32_t g_last_read_ms = 0;

/* ── 1-Wire low-level helpers ──────────────────────────────────────── */

static inline void ow_low(void)
{
    gpio_set_level(g_ow_pin, 0);
}

static inline void ow_release(void)
{
    gpio_set_level(g_ow_pin, 1);
}

static inline int ow_read_pin(void)
{
    return gpio_get_level(g_ow_pin);
}

static inline void ow_set_output(void)
{
    gpio_set_direction(g_ow_pin, GPIO_MODE_OUTPUT);
}

static inline void ow_set_input(void)
{
    gpio_set_direction(g_ow_pin, GPIO_MODE_INPUT);
}

/* ── 1-Wire reset + presence ────────────────────────────────────────── */
static bool ow_reset(void)
{
    portDISABLE_INTERRUPTS();
    ow_set_output();
    ow_low();
    esp_rom_delay_us(OW_RESET_LOW_US);
    ow_set_input();    /* release — pull-up takes bus HIGH */
    esp_rom_delay_us(OW_RESET_WAIT_US);
    int presence = ow_read_pin();  /* 0 = device present */
    esp_rom_delay_us(OW_PRESENCE_WAIT);
    portENABLE_INTERRUPTS();
    return (presence == 0);
}

/* ── Write one bit ──────────────────────────────────────────────────── */
static void ow_write_bit(int bit)
{
    portDISABLE_INTERRUPTS();
    ow_set_output();
    ow_low();
    if (bit) {
        esp_rom_delay_us(OW_WRITE_1_LOW_US);
        ow_set_input();   /* release early for write-1 */
        esp_rom_delay_us(OW_WRITE_0_LOW_US);
    } else {
        esp_rom_delay_us(OW_WRITE_0_LOW_US);
        ow_set_input();
        esp_rom_delay_us(OW_RECOVERY_US);
    }
    portENABLE_INTERRUPTS();
}

static void ow_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

/* ── Read one bit ───────────────────────────────────────────────────── */
static int ow_read_bit(void)
{
    int bit;
    portDISABLE_INTERRUPTS();
    ow_set_output();
    ow_low();
    esp_rom_delay_us(OW_READ_LOW_US);
    ow_set_input();
    esp_rom_delay_us(OW_READ_SAMPLE_US);
    bit = ow_read_pin();
    esp_rom_delay_us(OW_WRITE_0_LOW_US - OW_READ_SAMPLE_US);
    portENABLE_INTERRUPTS();
    return bit;
}

static uint8_t ow_read_byte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (ow_read_bit()) byte |= 0x80;
    }
    return byte;
}

/* ── DS18B20 scratchpad CRC (Dallas 1-Wire CRC-8) ─────────────────── */
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

/* ── Trigger conversion ─────────────────────────────────────────────── */
static bool ds18b20_convert(void)
{
    if (!ow_reset()) {
        ESP_LOGW(TAG, "Reset failed (convert)");
        return false;
    }
    ow_write_byte(DS18B20_CMD_SKIP_ROM);
    ow_write_byte(DS18B20_CMD_CONVERT_T);
    return true;
}

/* ── Read scratchpad, check CRC, return temperature ─────────────────── */
static bool ds18b20_read_temp(float *out)
{
    uint8_t buf[9];
    if (!ow_reset()) {
        ESP_LOGW(TAG, "Reset failed (read)");
        return false;
    }
    ow_write_byte(DS18B20_CMD_SKIP_ROM);
    ow_write_byte(DS18B20_CMD_READ_SCRATCH);

    for (int i = 0; i < 9; i++) {
        buf[i] = ow_read_byte();
    }

    ESP_LOGD(TAG, "scratch: %02x %02x %02x %02x %02x %02x %02x %02x [%02x]",
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

    /* Detect floating bus — all bits read as 0 (CRC-8 of 8×0x00 = 0x00) */
    bool all_zero = true;
    for (int i = 0; i < 9; i++) {
        if (buf[i] != 0) { all_zero = false; break; }
    }
    if (all_zero) {
        ESP_LOGW(TAG, "All-zero scratchpad — bus floating? Check 4.7kΩ pull-up on GPIO%d", g_ow_pin);
        return false;
    }

    uint8_t calc_crc = crc8(buf, 8);
    if (calc_crc != buf[8]) {
        ESP_LOGW(TAG, "CRC mismatch: calc=0x%02x rcv=0x%02x", calc_crc, buf[8]);
        return false;
    }

    int16_t raw = (int16_t)(buf[1] << 8) | buf[0];
    *out = raw / 16.0f + g_cal_offset;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
   PUBLIC API
   ═══════════════════════════════════════════════════════════════════════ */

void ds18b20_init(void)
{
    ESP_LOGI(TAG, "Initializing DS18B20 on GPIO%d...", DS18B20_PIN);
    g_ow_pin = DS18B20_PIN;

    /* Open-drain emulation: output LOW or float (input) to let pull-up win.
     * Internal pull-up as fallback; external 4.7kΩ recommended for reliability. */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << g_ow_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);

    /* Load calibration offset from NVS */
    nvs_handle_t handle;
    if (nvs_open(DS18B20_NVS_NS, NVS_READONLY, &handle) == ESP_OK) {
        int32_t ofs_mdeg = 0;
        if (nvs_get_i32(handle, NVS_KEY_OFFSET, &ofs_mdeg) == ESP_OK) {
            g_cal_offset = ofs_mdeg / 1000.0f;
            ESP_LOGI(TAG, "Loaded offset from NVS: %.1f °C", g_cal_offset);
        }
        uint8_t calib = 0;
        if (nvs_get_u8(handle, NVS_KEY_CALIB, &calib) == ESP_OK && calib) {
            g_calibrated = true;
        }
        nvs_close(handle);
    }

    /* Initial presence check */
    if (ow_reset()) {
        ESP_LOGI(TAG, "DS18B20 detected");
    } else {
        ESP_LOGW(TAG, "DS18B20 NOT detected on GPIO%d — check wiring", g_ow_pin);
    }

    /* Set 12-bit resolution */
    uint8_t res = DS18B20_RESOLUTION;
    uint8_t th = 0, tl = 0;
    switch (res) {
    case 9:  th = 0x1F; tl = 0xE0; break;
    case 10: th = 0x3F; tl = 0xC0; break;
    case 11: th = 0x5F; tl = 0xA0; break;
    case 12: th = 0x7F; tl = 0x80; break;
    }
    if (ow_reset()) {
        ow_write_byte(DS18B20_CMD_SKIP_ROM);
        ow_write_byte(DS18B20_CMD_WRITE_SCRATCH);
        ow_write_byte(th);
        ow_write_byte(tl);
        ow_write_byte(0x7F); /* config: 12-bit */
        ESP_LOGI(TAG, "DS18B20 resolution set to %d-bit", res);
    }

    if (!g_calibrated) {
        ESP_LOGI(TAG, "No offset set. Use /calibrate?t=<value> to set a trim offset.");
    }

    ESP_LOGI(TAG, "DS18B20 ready");
}

float ds18b20_read(void)
{
    if (g_ow_pin < 0) return -999.0f;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Rate-limit: don't read faster than every 2 seconds.
     * DS18B20 conversion takes up to 750ms. */
    if (g_last_read_ms != 0 && (now - g_last_read_ms) < 2000) {
        /* Return last known value — caller should use callback for updates */
        return -999.0f;
    }

    if (!ds18b20_convert()) {
        if (g_callback) g_callback(-999.0f, g_cb_user_data);
        return -999.0f;
    }

    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERT_MS));

    float temp;
    if (!ds18b20_read_temp(&temp)) {
        ESP_LOGW(TAG, "Read failed");
        if (g_callback) g_callback(-999.0f, g_cb_user_data);
        return -999.0f;
    }

    g_last_read_ms = now;

    if (temp < -55.0f || temp > 125.0f) {
        ESP_LOGW(TAG, "Out of range: %.1f °C", temp);
        if (g_callback) g_callback(-999.0f, g_cb_user_data);
        return -999.0f;
    }

    ESP_LOGI(TAG, "Temp: %.1f °C", temp);
    if (g_callback) g_callback(temp, g_cb_user_data);
    return temp;
}

void ds18b20_set_callback(ds18b20_data_cb_t cb, void *user_data)
{
    g_callback = cb;
    g_cb_user_data = user_data;
}

bool ds18b20_calibrate(float known_temp_c)
{
    if (g_ow_pin < 0) return false;

    if (!ds18b20_convert()) return false;
    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERT_MS));

    float raw;
    if (!ds18b20_read_temp(&raw)) return false;

    /* Calculate offset: trim = known - raw, then apply to g_cal_offset */
    float old_offset = g_cal_offset;
    float raw_no_offset = raw - old_offset;
    g_cal_offset = known_temp_c - raw_no_offset;

    ESP_LOGI(TAG, "Calibrated: raw=%.1f ref=%.1f offset=%.2f °C",
             raw_no_offset, known_temp_c, g_cal_offset);

    g_calibrated = true;

    nvs_handle_t handle;
    if (nvs_open(DS18B20_NVS_NS, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i32(handle, NVS_KEY_OFFSET, (int32_t)(g_cal_offset * 1000.0f));
        nvs_set_u8(handle, NVS_KEY_CALIB, 1);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Offset saved to NVS");
    }
    return true;
}

bool ds18b20_is_calibrated(void)
{
    return g_calibrated;
}
