#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
    uint8_t gesture;
} touch_data_t;

typedef void (*touch_cb_t)(const touch_data_t *data, void *user_data);

void touch_init(void);
bool touch_read(touch_data_t *out);
void touch_set_callback(touch_cb_t cb, void *user_data);
