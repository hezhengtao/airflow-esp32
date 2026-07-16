#include "app_controller.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Smart Air Purifier starting...");

    app_controller_init();
    app_controller_start();
}
