set(UI_DIR ./ui)
file(GLOB_RECURSE UI_SRCS ${UI_DIR}/*.c)  # font_cjk_28/48 added

set(HOLIDAY_DIR ./holiday)
file(GLOB_RECURSE HOLIDAY_SRCS ${HOLIDAY_DIR}/*.c)

set(ASSETS_DIR ./assets)
file(GLOB_RECURSE ASSETS_SRCS ${ASSETS_DIR}/*.c)

idf_component_register(
    SRCS
        "main.c"
        "app_controller.c"
        "event_bus.c"
        ${UI_SRCS}
        ${HOLIDAY_SRCS}
        ${ASSETS_SRCS}
    INCLUDE_DIRS
        "."
        ${UI_DIR}
        ${HOLIDAY_DIR}
    REQUIRES
        freertos
        esp_timer
        esp_driver_gpio
        driver
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        lwip
        esp_adc
        esp_lcd
        mqtt
        status_led
        factory_reset
        settings
        motor_bldc
        sensor_y01
        sensor_ds18b20
        mqtt_ha
        wifi_prov
        sensor_history
        speaker
        touch_ft5x06
        lcd_tk043f1509
        esp_lcd_touch_ft5x06
        esp_lvgl_port
        esp_driver_i2c
        lvgl
        esp_http_client
        cjson
)
