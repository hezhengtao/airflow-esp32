idf_component_register(
    SRCS "speaker.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio esp_driver_ledc freertos esp_timer
)
