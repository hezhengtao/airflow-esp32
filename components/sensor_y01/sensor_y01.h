#pragma once

#include <stdint.h>

typedef struct {
    uint16_t tvoc_ugm3;
    uint16_t co2_ppm;
    uint16_t ch2o_ugm3;  /* μg/m³ */
    bool valid;
} y01_data_t;

typedef void (*y01_data_cb_t)(const y01_data_t *data, void *user_data);

void y01_init(void);
void y01_set_callback(y01_data_cb_t cb, void *user_data);
void y01_read_manual(void);  /* trigger single read in Q&A mode */
const y01_data_t *y01_get_latest(void);
