/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch: FT5x06
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new FT5x06 touch driver
 *
 * @note The I2C communication should be initialized before use this function.
 *
 * @param io LCD/Touch panel IO handle
 * @param config: Touch configuration
 * @param out_touch: Touch instance handle
 * @return
 *      - ESP_OK                    on success
 *      - ESP_ERR_NO_MEM            if there is no memory for allocating main structure
 */
esp_err_t esp_lcd_touch_new_i2c_ft5x06(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config,
                                       esp_lcd_touch_handle_t *out_touch);

/**
 * @brief Set a direct I2C device handle for the touch driver to use.
 *
 * When set, the driver uses separate i2c_master_transmit + i2c_master_receive
 * (STOP between write and read) instead of i2c_master_transmit_receive
 * (repeated START). Some touch chips (e.g. FT6336) require STOP between.
 *
 * @param touch Touch handle
 * @param i2c_dev Direct I2C master device handle (at address 0x38)
 */
void esp_lcd_touch_ft5x06_set_i2c_dev(esp_lcd_touch_handle_t touch, void *i2c_dev);

/**
 * @brief I2C address of the FT5x06 controller
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS (0x38)

/**
 * @brief Touch IO configuration structure
 *
 */
#define ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()                \
    {                                                       \
        .scl_speed_hz = 100000,                             \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,    \
        .control_phase_bytes = 1,                           \
        .dc_bit_offset = 0,                                 \
        .lcd_cmd_bits = 8,                                  \
        .flags =                                            \
        {                                                   \
            .disable_control_phase = 1,                     \
        }                                                   \
    }


#ifdef __cplusplus
}
#endif
