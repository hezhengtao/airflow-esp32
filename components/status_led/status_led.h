#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LED_EFFECT_STEADY = 0,
    LED_EFFECT_BLINK = 1,
    LED_EFFECT_BREATHE = 2,
    LED_EFFECT_FAST_BLINK = 3,
    LED_EFFECT_RAINBOW = 4,
} led_effect_t;

void status_led_init(void);
void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void status_led_set_brightness(uint8_t pct);
void status_led_set_on(bool on);
bool status_led_is_on(void);
void status_led_set_effect(led_effect_t effect);
