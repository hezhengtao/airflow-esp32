#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (*ds18b20_data_cb_t)(float temp_celsius, void *user_data);

void ds18b20_init(void);
void ds18b20_set_callback(ds18b20_data_cb_t cb, void *user_data);
float ds18b20_read(void);
bool ds18b20_calibrate(float known_temp_c);
bool ds18b20_is_calibrated(void);
