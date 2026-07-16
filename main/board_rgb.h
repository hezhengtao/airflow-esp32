#pragma once
/*
 * board_rgb.h — RGB 16-bit parallel interface pin mapping
 *
 * 用法：
 *   1. 将此文件复制为 board.h（备份原有 board.h）
 *   2. 按下面的引脚表重新连接杜邦线
 *   3. 将 LCD FPC 的 IM0/IM1 设置为 HIGH（选择 RGB 模式）
 *      - IM0 (FPC pin 3): 接 3.3V
 *      - IM1 (FPC pin 4): 接 3.3V
 *   4. 编译并烧录
 *
 * 硬件连接表（ESP32-S3 ← → LCD FPC）：
 *   GPIO 2  → FPC pin 37 (PCLK)
 *   GPIO 42 → FPC pin 38 (HSYNC)
 *   GPIO 41 → FPC pin 39 (VSYNC)
 *   GPIO 17 → FPC pin 36 (DE)
 *   GPIO 4  → FPC pin 12 (DB0)
 *   GPIO 5  → FPC pin 13 (DB1)
 *   GPIO 6  → FPC pin 14 (DB2)
 *   GPIO 7  → FPC pin 15 (DB3)
 *   GPIO 15 → FPC pin 16 (DB4)
 *   GPIO 16 → FPC pin 17 (DB5)
 *   GPIO 18 → FPC pin 18 (DB6)
 *   GPIO 8  → FPC pin 19 (DB7)
 *   GPIO 10 → FPC pin 20 (DB8)
 *   GPIO 11 → FPC pin 21 (DB9)
 *   GPIO 12 → FPC pin 22 (DB10)
 *   GPIO 14 → FPC pin 23 (DB11)
 *   GPIO 21 → FPC pin 24 (DB12)
 *   GPIO 45 → FPC pin 25 (DB13)
 *   GPIO 47 → FPC pin 26 (DB14)
 *   GPIO 1  → FPC pin 27 (DB15)
 *   GPIO 46 → FPC pin 8  (RESET)
 *   GPIO 19 → FPC pin 44 (LEDA - 背光)
 *   GPIO 20 → Touch SDA (I2C)
 *   GPIO 13 → Touch SCL (I2C)
 *
 * 注意：
 *   - GPIO 8 原用于 PT1000，测试 RGB 时需要断开
 *   - GPIO 9 (Y01 UART RX) 在 RGB 模式下空闲
 *   - 传感器（Y01, PT1000）在 RGB 测试中不初始化
 *   - 使用 16 条数据线（DB0-DB15），RGB565 格式
 *   - 每像素 16 bit，占用 2 字节
 *   - DB16-DB23 未连接（不用于 RGB565）
 */

#include "driver/gpio.h"

/* ── LCD resolution ──────────────────────────────────────────────── */
#define LCD_WIDTH           800   /* physical landscape */
#define LCD_HEIGHT          480

/* ── RGB sync signals ────────────────────────────────────────────── */
#define LCD_RGB_PCLK        GPIO_NUM_2    /* → FPC pin 37 */
#define LCD_RGB_HSYNC       GPIO_NUM_42   /* → FPC pin 38 */
#define LCD_RGB_VSYNC       GPIO_NUM_41   /* → FPC pin 39 */
#define LCD_RGB_DE          GPIO_NUM_17   /* → FPC pin 36 */
#define LCD_RGB_DISP        GPIO_NUM_NC   /* not connected */

/* ── RGB data lines (16-bit, RGB565) ─────────────────────────────── */
#define LCD_RGB_D0          GPIO_NUM_4    /* → FPC pin 12 */
#define LCD_RGB_D1          GPIO_NUM_5    /* → FPC pin 13 */
#define LCD_RGB_D2          GPIO_NUM_6    /* → FPC pin 14 */
#define LCD_RGB_D3          GPIO_NUM_7    /* → FPC pin 15 */
#define LCD_RGB_D4          GPIO_NUM_15   /* → FPC pin 16 */
#define LCD_RGB_D5          GPIO_NUM_16   /* → FPC pin 17 */
#define LCD_RGB_D6          GPIO_NUM_18   /* → FPC pin 18 */
#define LCD_RGB_D7          GPIO_NUM_8    /* → FPC pin 19 */
#define LCD_RGB_D8          GPIO_NUM_10   /* → FPC pin 20 */
#define LCD_RGB_D9          GPIO_NUM_11   /* → FPC pin 21 */
#define LCD_RGB_D10         GPIO_NUM_12   /* → FPC pin 22 */
#define LCD_RGB_D11         GPIO_NUM_14   /* → FPC pin 23 */
#define LCD_RGB_D12         GPIO_NUM_21   /* → FPC pin 24 */
#define LCD_RGB_D13         GPIO_NUM_45   /* → FPC pin 25 */
#define LCD_RGB_D14         GPIO_NUM_47   /* → FPC pin 26 */
#define LCD_RGB_D15         GPIO_NUM_1    /* → FPC pin 27 */

/* ── Shared pins (same as i80 mode) ──────────────────────────────── */
#define LCD_RST_PIN         GPIO_NUM_46   /* → FPC pin 8 */
#define LCD_BL_PIN          GPIO_NUM_19   /* → FPC pin 44 (LEDA) */
#define LCD_BL_FREQ_HZ      5000

/* ── Touch (unchanged) ───────────────────────────────────────────── */
#define TOUCH_I2C_PORT      I2C_NUM_1
#define TOUCH_I2C_SDA       GPIO_NUM_20
#define TOUCH_I2C_SCL       GPIO_NUM_13
#define TOUCH_I2C_FREQ_HZ   400000
#define TOUCH_INT_PIN       GPIO_NUM_NC
#define TOUCH_RST_PIN       GPIO_NUM_NC
#define TOUCH_I2C_ADDR      0x38

/* ── Motor, Sensor pins ──────────────────────────────────────────── */
/* NOTE: PT1000 (GPIO8) is repurposed as D7. Y01 (GPIO9) is free.
   Sensors are disabled during RGB testing — do not connect them. */
#define MOTOR_START_PIN     GPIO_NUM_38
#define MOTOR_PWM_PIN       GPIO_NUM_39
#define MOTOR_SPEED_PIN     GPIO_NUM_40
#define MOTOR_PWM_FREQ_HZ   25000
#define MOTOR_PWM_RES       LEDC_TIMER_10_BIT
#define MOTOR_PWM_MAX_DUTY  1023
#define MOTOR_PULSES_PER_REV 6

#define Y01_UART_PORT       UART_NUM_1
#define Y01_UART_TX         GPIO_NUM_41   /* WARNING: conflicts with VSYNC */
#define Y01_UART_RX         GPIO_NUM_9
#define Y01_UART_BAUD       9600

#define PT1000_ADC_UNIT     ADC_UNIT_1
#define PT1000_ADC_CH       ADC_CHANNEL_7
#define PT1000_ADC_PIN      GPIO_NUM_8    /* WARNING: repurposed as D7 */
#define PT1000_REF_RESISTOR 1000.0f

#define SPEAKER_PWM_PIN     GPIO_NUM_48
#define SPEAKER_PWM_FREQ    44100
#define SPEAKER_PWM_RES     LEDC_TIMER_8_BIT

/* ── NVS ─────────────────────────────────────────────────────────── */
#define NVS_NAMESPACE       "air_purifier"
#define NVS_KEY_WIFI_SSID   "wifi_ssid"
#define NVS_KEY_WIFI_PASS   "wifi_pass"
#define NVS_KEY_MQTT_URI    "mqtt_uri"
#define NVS_KEY_MQTT_USER   "mqtt_user"
#define NVS_KEY_MQTT_PASS   "mqtt_pass"
#define NVS_KEY_MOTOR_MIN   "motor_min"
#define NVS_KEY_MOTOR_MAX   "motor_max"
#define NVS_KEY_TVOC_ALARM  "tvoc_alarm"
#define NVS_KEY_CO2_ALARM   "co2_alarm"
#define NVS_KEY_CH2O_ALARM  "ch2o_alarm"
#define NVS_KEY_AUTO_FAN_ENABLE "auto_fan"
#define NVS_KEY_BRIGHTNESS      "brightness"
#define NVS_KEY_CHILD_LOCK  "child_lock"
#define NVS_KEY_BOOT_COUNT  "boot_count"
#define NVS_KEY_BOOT_TIME   "boot_time"
#define NVS_KEY_DEVICE_NAME "dev_name"
#define NVS_KEY_DEVICE_ID   "device_id"
