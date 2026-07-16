#include "sensor_y01.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "y01";

static y01_data_t g_latest = {0};
static y01_data_cb_t g_callback = NULL;
static void *g_cb_user_data = NULL;

#define Y01_BUF_SIZE    32
#define Y01_FRAME_SIZE  9

/*
 * Y01 protocol (active upload, 9600 8N1).
 * Frame format (9 bytes):
 *   [0]    0x2C  — fixed address high
 *   [1]    0xE4  — fixed address low
 *   [2]    TVOC high byte (μg/m³)
 *   [3]    TVOC low byte
 *   [4]    CH2O high byte (μg/m³)
 *   [5]    CH2O low byte
 *   [6]    CO2 high byte (ppm)
 *   [7]    CO2 low byte
 *   [8]    checksum (uint8 sum of bytes 0..7)
 *
 * Conversion: TVOC/CH2O = (high*256+low)*0.001 mg/m³ = raw μg/m³
 *             CO2 = high*256+low ppm
 */
static uint8_t calc_checksum(const uint8_t *buf, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; i++) sum += buf[i];
    return sum & 0xFF;
}

static bool parse_frame(const uint8_t *frame, y01_data_t *out)
{
    if (frame[0] != 0x2C || frame[1] != 0xE4) return false;
    if (frame[8] != calc_checksum(frame, 8)) return false;

    out->tvoc_ugm3 = ((uint16_t)frame[2] << 8) | frame[3];
    out->ch2o_ugm3 = ((uint16_t)frame[4] << 8) | frame[5];
    out->co2_ppm   = ((uint16_t)frame[6] << 8) | frame[7];
    out->valid = true;
    return true;
}

static void y01_task(void *arg)
{
    uint8_t buf[Y01_BUF_SIZE];
    uint8_t frame[Y01_FRAME_SIZE];
    int frame_idx = 0;
    bool synced = false;
    uint32_t err_count = 0;
    uint32_t frame_count = 0;

    while (1) {
        int len = uart_read_bytes(Y01_UART_PORT, buf, sizeof(buf),
                                  pdMS_TO_TICKS(200));
        if (len < 0) {
            err_count++;
            if (err_count == 1 || err_count % 500 == 0) {
                ESP_LOGW(TAG, "UART read error (sensor not connected?)");
            }
            synced = false;
            frame_idx = 0;
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        if (len == 0) {
            synced = false;
            frame_idx = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for (int i = 0; i < len; i++) {
            uint8_t b = buf[i];

            if (!synced) {
                if (b == 0x2C) {
                    synced = true;
                    frame_idx = 0;
                    frame[frame_idx++] = b;
                }
                continue;
            }

            /* After 0x2C, verify second byte is 0xE4 */
            if (frame_idx == 1 && b != 0xE4) {
                synced = false;
                frame_idx = 0;
                if (b == 0x2C) {
                    synced = true;
                    frame[frame_idx++] = b;
                }
                continue;
            }

            frame[frame_idx++] = b;
            if (frame_idx == Y01_FRAME_SIZE) {
                y01_data_t parsed;
                if (parse_frame(frame, &parsed)) {
                    frame_count++;
                    g_latest = parsed;
                    if (frame_count <= 3 || frame_count % 100 == 0) {
                        ESP_LOGI(TAG, "#%lu TVOC=%u CO2=%u CH2O=%u",
                                 frame_count, parsed.tvoc_ugm3,
                                 parsed.co2_ppm, parsed.ch2o_ugm3);
                    }
                    if (g_callback) g_callback(&g_latest, g_cb_user_data);
                } else {
                    ESP_LOGW(TAG, "Checksum mismatch: "
                             "%02X %02X %02X %02X %02X %02X %02X %02X %02X  "
                             "(calc=%02X expect=%02X)",
                             frame[0], frame[1], frame[2], frame[3], frame[4],
                             frame[5], frame[6], frame[7], frame[8],
                             calc_checksum(frame, 8), frame[8]);
                }
                synced = false;
                frame_idx = 0;
            }
        }
    }
}

void y01_init(void)
{
    ESP_LOGI(TAG, "Initializing Y01 sensor...");

    uart_config_t uart_cfg = {
        .baud_rate = Y01_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(Y01_UART_PORT, 256,
                                         0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "UART driver install failed: %s — sensor unavailable", esp_err_to_name(err));
        return;
    }

    err = uart_param_config(Y01_UART_PORT, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "UART param config failed: %s", esp_err_to_name(err));
        uart_driver_delete(Y01_UART_PORT);
        return;
    }

    err = uart_set_pin(Y01_UART_PORT, Y01_UART_TX, Y01_UART_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        uart_driver_delete(Y01_UART_PORT);
        return;
    }

    gpio_set_pull_mode(Y01_UART_RX, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "Y01 pins: RX=GPIO%d TX=GPIO%d", Y01_UART_RX, Y01_UART_TX);

    if (xTaskCreate(y01_task, "y01_task", 2560, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Failed to create Y01 task");
        uart_driver_delete(Y01_UART_PORT);
        return;
    }
    ESP_LOGI(TAG, "Y01 sensor ready");
}

void y01_set_callback(y01_data_cb_t cb, void *user_data)
{
    g_callback = cb;
    g_cb_user_data = user_data;
}

void y01_read_manual(void)
{
    const uint8_t cmd[] = {0xFF, 0x01, 0x86};
    uart_write_bytes(Y01_UART_PORT, cmd, sizeof(cmd));
}

const y01_data_t *y01_get_latest(void)
{
    return &g_latest;
}
