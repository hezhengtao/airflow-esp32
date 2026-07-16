#pragma once

#include "driver/gpio.h"

/* ── LCD (NT35510, 8-bit parallel 8080, pinout from TK043F1509 reference) ── */
#define LCD_D0_PIN          GPIO_NUM_4
#define LCD_D1_PIN          GPIO_NUM_5
#define LCD_D2_PIN          GPIO_NUM_6
#define LCD_D3_PIN          GPIO_NUM_7
#define LCD_D4_PIN          GPIO_NUM_15
#define LCD_D5_PIN          GPIO_NUM_16
#define LCD_D6_PIN          GPIO_NUM_17
#define LCD_D7_PIN          GPIO_NUM_18
#define LCD_CS_PIN          GPIO_NUM_1
#define LCD_RS_PIN          GPIO_NUM_42   /* DC — Data/Command (Register Select) */
#define LCD_WR_PIN          GPIO_NUM_2    /* PCLK used as WR in i80 mode */
#define LCD_RD_PIN          GPIO_NUM_41   /* not used (tied to 3.3V) */
#define LCD_RST_PIN         GPIO_NUM_46
#define LCD_BL_PIN          GPIO_NUM_19    /* LCD backlight PWM */

#define LCD_PIN_MASK_DATA   ((1ULL << LCD_D0_PIN) | (1ULL << LCD_D1_PIN) | \
                             (1ULL << LCD_D2_PIN) | (1ULL << LCD_D3_PIN) | \
                             (1ULL << LCD_D4_PIN) | (1ULL << LCD_D5_PIN) | \
                             (1ULL << LCD_D6_PIN) | (1ULL << LCD_D7_PIN))

#define LCD_WIDTH           800   /* physical: 800×480 landscape, rotated to portrait via MADCTL */
#define LCD_HEIGHT          480
#define LCD_BL_FREQ_HZ      50000
#define LCD_BL_DUTY_MAX     100

/* ── Touch (FT5x06, I2C, pinout from TK043F1509 reference) ──────── */
#define TOUCH_I2C_PORT      I2C_NUM_1
#define TOUCH_I2C_SDA       GPIO_NUM_20
#define TOUCH_I2C_SCL       GPIO_NUM_13   /* from ESP_LCD_TK043F1509_LVGL_touch ref */
#define TOUCH_I2C_FREQ_HZ   400000
#define TOUCH_INT_PIN       GPIO_NUM_NC
#define TOUCH_RST_PIN       GPIO_NUM_NC
#define TOUCH_I2C_ADDR      0x38

/* ── Motor (43F704S100 BLDC, CLK freq control) ───────────────────── */
#define MOTOR_BRK_PIN       GPIO_NUM_21    /* green BRK, float per seller (unused by FW) */
#define MOTOR_CLK_PIN       GPIO_NUM_39    /* white CLK, 0–1kHz = 0–1900RPM (MCPWM hardware output) */
#define MOTOR_FG_PIN        GPIO_NUM_40    /* yellow FG, speed feedback */
#define MOTOR_CLK_FREQ_MAX  1000           /* 1kHz = rated 1900 RPM */
#define MOTOR_PWM_RES       LEDC_TIMER_8_BIT
#define MOTOR_PWM_MAX_DUTY  255
#define MOTOR_PCNT_UNIT     PCNT_UNIT_0
#define MOTOR_PULSES_PER_REV 6

/* ── Y01 Air Quality Sensor (UART) ──────────────────────────────── */
#define Y01_UART_PORT       UART_NUM_1
#define Y01_UART_TX         GPIO_NUM_41
#define Y01_UART_RX         GPIO_NUM_9     /* was GPIO42 — avoid conflict with LCD_RS */
#define Y01_UART_BAUD       9600

/* ── DS18B20 Temperature Sensor (1-Wire) ──────────────────────────── */
#define DS18B20_PIN         GPIO_NUM_8   /* DQ — data line, 4.7kΩ pull-up to 3.3V */

/* ── Status LED (WS2812-2020 RGB on GPIO38) ─────────────────────── */
#define WS2812_LED_PIN      GPIO_NUM_38    /* WS2812 DIN — data input */

/* ── Speaker (8Ω 2W, PAM8302 Class-D amp via RC filter) ──────────── */
#define SPEAKER_PWM_PIN     GPIO_NUM_48    /* LEDC PWM → RC LPF → PAM8302 → 8Ω speaker (BTL, no DC-block needed) */
/* NOTE: GPIO43/44 are hard-wired to USB-Serial-JTAG on ESP32-S3 —
   DO NOT use them for peripherals or console output will die! */
#define SPEAKER_PWM_FREQ    44100          /* CD-quality PWM carrier for tone generation */
#define SPEAKER_PWM_RES     LEDC_TIMER_8_BIT
#define SPEAKER_SHDN_PIN    GPIO_NUM_47    /* PAM8302 SHDN — LOW=off HIGH=on */

/* ── Passive components (soldered in-line with Dupont wires, no PCB) ── */
/* I2C pull-ups:       4.7kΩ ×2 (SDA→3.3V, SCL→3.3V) */
/* Motor START pulldown: 4.7kΩ (GPIO21→GND, was GPIO38 before LED move) */
/* WS2812 DIN: GPIO38 — NO pull resistors (RMT push-pull output direct to LED) */
/* Speaker amp:        PAM8302 IN+ ← GPIO48 via 100Ω+10nF RC LPF
                         PAM8302 IN- ← GND
                         PAM8302 SHDN ← GPIO47 */
/* DS18B20 1-Wire:    GPIO8 DQ + 4.7kΩ pull-up to 3.3V */
/* Power decoupling:   100nF ceramic ×4 (ESP32 3.3V, motor 12V, Y01 5V, LCD 5V) */
/* Motor bulk cap:     470µF/25V low-ESR electrolytic (across motor 12V) */

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
#define NVS_KEY_CH2O_ALARM      "ch2o_alarm"
#define NVS_KEY_AUTO_FAN_ENABLE "auto_fan"
#define NVS_KEY_BRIGHTNESS      "brightness"
#define NVS_KEY_CHILD_LOCK  "child_lock"
#define NVS_KEY_BOOT_COUNT  "boot_count"
#define NVS_KEY_BOOT_TIME   "boot_time"
#define NVS_KEY_DEVICE_NAME "dev_name"
#define NVS_KEY_DEVICE_ID   "device_id"
#define NVS_KEY_KEY_VOLUME      "key_vol"
#define NVS_KEY_KEY_SOUND_EN    "key_snd_en"
#define NVS_KEY_POWER_VOLUME    "pwr_vol"
#define NVS_KEY_POWER_SOUND_EN  "pwr_snd_en"
#define NVS_KEY_HOME_SCREEN     "home_scr"
#define NVS_KEY_KEY_MELODY     "key_mel"
#define NVS_KEY_PWRON_MELODY   "pwon_mel"
#define NVS_KEY_PWROFF_MELODY  "pwoff_mel"
#define NVS_KEY_ALARM_MELODY   "alarm_mel"
#define NVS_KEY_ALARM_VOLUME   "alarm_vol"
#define NVS_KEY_ALARM_SOUND_EN "alarm_snd"
#define NVS_KEY_ALARM_COOLDOWN "alarm_cd"
#define NVS_KEY_LED_ON          "led_on"
#define NVS_KEY_LED_R           "led_r"
#define NVS_KEY_LED_G           "led_g"
#define NVS_KEY_LED_B           "led_b"
#define NVS_KEY_LED_BRIGHT      "led_bri"
/* Per-state LED config: normal/alarm/shutdown/wifi_fail/wifi_conn */
#define NVS_KEY_LED_N_R         "led_nr"
#define NVS_KEY_LED_N_G         "led_ng"
#define NVS_KEY_LED_N_B         "led_nb"
#define NVS_KEY_LED_N_EFF       "led_ne"
#define NVS_KEY_LED_A_R         "led_ar"
#define NVS_KEY_LED_A_G         "led_ag"
#define NVS_KEY_LED_A_B         "led_ab"
#define NVS_KEY_LED_A_EFF       "led_ae"
#define NVS_KEY_LED_S_R         "led_sr"
#define NVS_KEY_LED_S_G         "led_sg"
#define NVS_KEY_LED_S_B         "led_sb"
#define NVS_KEY_LED_S_EFF       "led_se"
#define NVS_KEY_LED_F_R         "led_fr"
#define NVS_KEY_LED_F_G         "led_fg"
#define NVS_KEY_LED_F_B         "led_fb"
#define NVS_KEY_LED_F_EFF       "led_fe"
#define NVS_KEY_LED_C_R         "led_cr"
#define NVS_KEY_LED_C_G         "led_cg"
#define NVS_KEY_LED_C_B         "led_cb"
#define NVS_KEY_LED_C_EFF       "led_ce"
#define NVS_KEY_LED_O_R         "led_or"
#define NVS_KEY_LED_O_G         "led_og"
#define NVS_KEY_LED_O_B         "led_ob"
#define NVS_KEY_LED_O_EFF       "led_oe"
#define NVS_KEY_SCHED_OFF_EN    "sched_off_en"
#define NVS_KEY_SCHED_ON_EN     "sched_on_en"
#define NVS_KEY_SCHED_EN        "sched_en"  /* legacy — kept for migration */
#define NVS_KEY_SCHED_OFF_H     "sched_off_h"
#define NVS_KEY_SCHED_OFF_M     "sched_off_m"
#define NVS_KEY_SCHED_ON_H      "sched_on_h"
#define NVS_KEY_SCHED_ON_M      "sched_on_m"
#define NVS_KEY_SCHED_OFF_DAY   "sch_off_d"
#define NVS_KEY_SCHED_ON_DAY    "sch_on_d"
#define NVS_KEY_SCHED_OFF_DATE  "sch_off_dt"
#define NVS_KEY_SCHED_ON_DATE   "sch_on_dt"
#define NVS_KEY_SCHED_OFF_MASK  "sch_off_mk"  /* bitmask for mode 13 (自定义) */
#define NVS_KEY_SCHED_ON_MASK   "sch_on_mk"
#define NVS_KEY_WEB_SCHEDULES   "web_scheds"  /* JSON array of web-defined schedules */
