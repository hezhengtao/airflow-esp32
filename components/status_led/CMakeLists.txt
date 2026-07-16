idf_component_register(
    SRCS "status_led.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_rmt esp_driver_gpio
)
