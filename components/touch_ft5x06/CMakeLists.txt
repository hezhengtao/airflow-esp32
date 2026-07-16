idf_component_register(
    SRCS "touch_ft5x06.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio esp_driver_i2c freertos
)
