idf_component_register(
    SRCS "sensor_ds18b20.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio freertos nvs_flash
)
