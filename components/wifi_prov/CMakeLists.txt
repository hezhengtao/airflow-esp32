idf_component_register(
    SRCS "wifi_prov.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../main" "../../components/settings"
    REQUIRES esp_driver_gpio nvs_flash esp_wifi esp_event esp_http_server lwip settings mqtt_ha app_update sensor_history sensor_ds18b20 motor_bldc
)
