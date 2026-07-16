idf_component_register(
    SRCS "sensor_y01.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio esp_driver_uart freertos
)
