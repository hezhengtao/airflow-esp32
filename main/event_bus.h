#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EVENT_NONE = 0,
    EVENT_SENSOR_Y01_UPDATE,
    EVENT_SENSOR_TEMP_UPDATE,
    EVENT_MOTOR_STATE_CHANGE,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_MQTT_CONNECTED,
    EVENT_MQTT_DISCONNECTED,
    EVENT_MQTT_CMD_RECEIVED,
    EVENT_FACTORY_RESET,
    EVENT_SETTINGS_CHANGED,
    EVENT_CHILD_LOCK_TOGGLE,
    EVENT_ALARM_TRIGGERED,
    EVENT_MAX
} event_id_t;

typedef struct {
    event_id_t id;
    union {
        struct { uint16_t tvoc; uint16_t co2; uint16_t ch2o; } y01;
        struct { float temp_c; } temp;
        struct { uint8_t state; uint16_t rpm; } motor;
        struct { char topic[64]; char payload[256]; } mqtt_cmd;
        struct { char ssid[33]; } wifi;
        struct { uint8_t type; uint16_t value; uint16_t threshold; } alarm;
    } data;

    #define ALARM_TYPE_TVOC  0
    #define ALARM_TYPE_CO2   1
    #define ALARM_TYPE_CH2O  2
} event_t;

typedef void (*event_handler_t)(const event_t *event, void *user_data);

void event_bus_init(void);
bool event_bus_subscribe(event_id_t id, event_handler_t handler, void *user_data);
bool event_bus_unsubscribe(event_id_t id, event_handler_t handler);
void event_bus_publish(const event_t *event);
