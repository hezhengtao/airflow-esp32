idf_component_register(
    SRCS "mqtt_ha.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio mqtt nvs_flash
)
