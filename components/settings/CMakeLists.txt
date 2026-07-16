idf_component_register(
    SRCS "settings.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio nvs_flash
)
