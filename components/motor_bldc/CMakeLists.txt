idf_component_register(
    SRCS "motor_bldc.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio esp_driver_mcpwm esp_driver_pcnt freertos
    # MCPWM drives CLK/BRK via hardware — no ISR, no LEDC, no GPTimer
)
