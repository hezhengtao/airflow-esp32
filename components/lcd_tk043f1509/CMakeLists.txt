idf_component_register(
    SRCS "lcd_tk043f1509.c" "LCD_TK040F1510.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_driver_gpio esp_driver_ledc esp_lcd
)
