#include "touch_ft5x06.h"
#include "board.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

/* FT6206/FT5x06 register offsets within the 32-byte read-from-0x00 block */
#define REG_TD_STATUS    0x02   /* touch points count (low nibble) */
#define REG_P1_YH        0x03   /* Y[11:8] + event flag */
#define REG_P1_YL        0x04   /* Y[7:0] */
#define REG_P1_XH        0x05   /* X[11:8] */
#define REG_P1_XL        0x06   /* X[7:0] */

static touch_cb_t g_callback = NULL;
static void *g_cb_user_data = NULL;
static i2c_master_dev_handle_t g_i2c_dev = NULL;

static esp_err_t ft5x06_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(g_i2c_dev, &reg, 1, data, len, 50);
}

static void touch_task(void *arg)
{
    touch_data_t data;
    uint8_t buf[32];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30));

        if (ft5x06_read_reg(0x00, buf, 32) != ESP_OK) continue;

        uint8_t points = buf[REG_TD_STATUS] & 0x0F;
        if (points < 1) continue;

        /* Parse touch point 1 — FT6206 format: Y first, then X */
        data.x = (int16_t)(buf[REG_P1_XH] & 0x0F) << 8 | buf[REG_P1_XL];
        data.y = (int16_t)(buf[REG_P1_YH] & 0x0F) << 8 | buf[REG_P1_YL];
        data.pressed = true;
        data.gesture = 0;

        /* Coordinate transform: swap X/Y and flip for portrait mode */
        uint16_t tmp = data.x;
        data.x = LCD_WIDTH - 1 - data.y;
        data.y = tmp;

        if (g_callback) g_callback(&data, g_cb_user_data);
    }
}

void touch_init(void)
{
    ESP_LOGI(TAG, "Initializing FT6206 touch controller");

    /* ── Pre-check: read SDA/SCL before touching I2C hardware ────── */
    /* Configure as GPIO inputs with internal pull-ups to detect device */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << TOUCH_I2C_SDA) | (1ULL << TOUCH_I2C_SCL),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_cfg);
    vTaskDelay(pdMS_TO_TICKS(10));  /* let pins settle */

    int sda = gpio_get_level(TOUCH_I2C_SDA);
    int scl = gpio_get_level(TOUCH_I2C_SCL);
    ESP_LOGI(TAG, "SDA=GPIO%d:%d SCL=GPIO%d:%d", TOUCH_I2C_SDA, sda, TOUCH_I2C_SCL, scl);

    if (sda == 0 || scl == 0) {
        ESP_LOGW(TAG, "I2C bus not connected (pins floating LOW) — touch disabled");
        ESP_LOGW(TAG, "Connect touch controller with 4.7kΩ pull-ups on SDA/SCL");
        /* Leave pins as GPIO inputs so they don't interfere */
        return;
    }
    ESP_LOGI(TAG, "I2C bus looks OK, proceeding with init");

    /* ── I2C master bus init ─────────────────────────────────────── */
    /* Reset pins to default before I2C takes them */
    gpio_reset_pin(TOUCH_I2C_SDA);
    gpio_reset_pin(TOUCH_I2C_SCL);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s (0x%X)", esp_err_to_name(err), err);
        return;
    }
    ESP_LOGI(TAG, "I2C bus ready on SDA=GPIO%d SCL=GPIO%d", TOUCH_I2C_SDA, TOUCH_I2C_SCL);

    /* ── Register FT5x06 directly (skip probe — known address) ────── */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDR,
        .scl_speed_hz = TOUCH_I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &g_i2c_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s (0x%X) — touch disabled", esp_err_to_name(err), err);
        return;
    }
    ESP_LOGI(TAG, "I2C device registered at 0x%02X", TOUCH_I2C_ADDR);

    /* Reset pulse */
    if (TOUCH_RST_PIN >= 0) {
        gpio_set_direction(TOUCH_RST_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(TOUCH_RST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(TOUCH_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    /* Verify I2C presence — try both common FT5x06 addresses */
    uint8_t fw_id = 0;
    bool found = false;
    uint8_t addrs[] = {0x38, 0x5D};
    for (int i = 0; i < 2; i++) {
        /* Re-register device at alternate address */
        if (g_i2c_dev) i2c_master_bus_rm_device(g_i2c_dev);
        i2c_device_config_t dev_cfg2 = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addrs[i],
            .scl_speed_hz = TOUCH_I2C_FREQ_HZ,
        };
        if (i2c_master_bus_add_device(bus_handle, &dev_cfg2, &g_i2c_dev) != ESP_OK) continue;
        if (ft5x06_read_reg(0x00, &fw_id, 1) == ESP_OK) {
            ESP_LOGI(TAG, "FT6206 found at 0x%02X, mode: 0x%02X", addrs[i], fw_id);
            found = true;
            break;
        }
        ESP_LOGW(TAG, "No device at I2C addr 0x%02X", addrs[i]);
    }

    if (!found) {
        ESP_LOGE(TAG, "FT6206 not found — check wiring: SDA=GPIO%d SCL=GPIO%d RST=GPIO%d INT=GPIO%d",
                 TOUCH_I2C_SDA, TOUCH_I2C_SCL, TOUCH_RST_PIN, TOUCH_INT_PIN);
    }

    /* Configure interrupt pin */
    if (TOUCH_INT_PIN >= 0) {
        gpio_set_direction(TOUCH_INT_PIN, GPIO_MODE_INPUT);
        gpio_set_pull_mode(TOUCH_INT_PIN, GPIO_PULLUP_ONLY);
    }

    xTaskCreate(touch_task, "touch_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Touch driver initialized");
}

bool touch_read(touch_data_t *out)
{
    uint8_t buf[32];
    if (ft5x06_read_reg(0x00, buf, 32) != ESP_OK) return false;

    uint8_t points = buf[REG_TD_STATUS] & 0x0F;
    out->pressed = (points > 0);
    if (out->pressed) {
        /* FT6206 format: Y first, then X */
        out->x = (int16_t)(buf[REG_P1_XH] & 0x0F) << 8 | buf[REG_P1_XL];
        out->y = (int16_t)(buf[REG_P1_YH] & 0x0F) << 8 | buf[REG_P1_YL];
        out->gesture = 0;

        /* Coordinate transform: swap X/Y and flip for portrait mode */
        uint16_t tmp = out->x;
        out->x = LCD_WIDTH - 1 - out->y;
        out->y = tmp;
    }
    return true;
}

void touch_set_callback(touch_cb_t cb, void *user_data)
{
    g_callback = cb;
    g_cb_user_data = user_data;
}
