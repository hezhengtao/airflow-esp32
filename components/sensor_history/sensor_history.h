#pragma once

#include <stdint.h>
#include <time.h>

typedef struct {
    time_t   ts;
    float    temp;
    uint16_t tvoc_ugm3;
    uint16_t co2_ppm;
    uint16_t ch2o_ugm3;
} sensor_sample_t;

#define SENSOR_HISTORY_MAX_SAMPLES 720   /* 12h at 60s interval */
#define SENSOR_HISTORY_QUERY_LIMIT  300  /* max per API response */

void sensor_history_init(void);
void sensor_history_add(time_t ts, float temp, uint16_t tvoc, uint16_t co2, uint16_t ch2o);
int  sensor_history_query(time_t since, sensor_sample_t *buf, int max_samples);
int  sensor_history_count(void);
