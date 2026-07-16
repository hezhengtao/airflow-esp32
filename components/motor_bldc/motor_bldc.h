#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_RAMPING_UP,
    MOTOR_STATE_RAMPING_DOWN,
    MOTOR_STATE_STALL,
    MOTOR_STATE_ERROR
} motor_state_t;

typedef void (*motor_state_cb_t)(motor_state_t state, uint16_t rpm, void *user_data);

void motor_init(void);
void motor_start(void);
void motor_stop(void);
void motor_set_speed(uint8_t duty_percent);
uint8_t motor_get_speed(void);
uint16_t motor_get_rpm(void);
motor_state_t motor_get_state(void);
void motor_set_state_callback(motor_state_cb_t cb, void *user_data);
