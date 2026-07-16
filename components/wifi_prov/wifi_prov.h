#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_PROV_STATE_IDLE = 0,
    WIFI_PROV_STATE_STARTED,
    WIFI_PROV_STATE_CRED_RECEIVED,
    WIFI_PROV_STATE_CONNECTING,
    WIFI_PROV_STATE_CONNECTED,
    WIFI_PROV_STATE_FAILED,
} wifi_prov_state_t;

typedef void (*wifi_prov_state_cb_t)(wifi_prov_state_t state, void *user_data);

typedef struct {
    float temp_c;
    uint16_t tvoc_ugm3;
    uint16_t co2_ppm;
    uint16_t ch2o_ugm3;
    uint16_t fan_rpm;
    bool fan_on;
    uint8_t fan_speed;
} wifi_prov_sensor_t;

typedef enum {
    WIFI_PROV_SENSOR_TEMP  = 1 << 0,
    WIFI_PROV_SENSOR_TVOC  = 1 << 1,
    WIFI_PROV_SENSOR_CO2   = 1 << 2,
    WIFI_PROV_SENSOR_CH2O  = 1 << 3,
    WIFI_PROV_SENSOR_FAN   = 1 << 4,
} wifi_prov_sensor_field_t;

void wifi_prov_init(void);
void wifi_prov_start(void);
void wifi_prov_stop(void);
wifi_prov_state_t wifi_prov_get_state(void);
bool wifi_prov_is_provisioned(void);
void wifi_prov_erase_config(void);
void wifi_prov_set_state_callback(wifi_prov_state_cb_t cb, void *user_data);

/* Normal-mode HTTP server (STA mode, device dashboard) */
void wifi_prov_http_start_normal(void);

/* Update sensor readings for the web dashboard */
void wifi_prov_update_sensors(const wifi_prov_sensor_t *data, uint32_t mask);
void wifi_prov_log(const char *tag, const char *fmt, ...);
int  wifi_prov_get_logs(char *buf, int max_len);
