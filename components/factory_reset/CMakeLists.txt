idf_component_register(
    SRCS "factory_reset.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio nvs_flash esp_timer
)
