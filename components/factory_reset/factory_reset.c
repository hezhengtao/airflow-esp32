#include "factory_reset.h"
#include "board.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char *TAG = "factory_reset";

#define RESET_WINDOW_SEC    30
#define RESET_THRESHOLD     3

static const char *reset_nvs = NVS_NAMESPACE;

bool factory_reset_check(void)
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open(reset_nvs, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t boot_count = 0;
    int64_t last_boot_time = 0;
    int64_t now = esp_timer_get_time() / 1000000;  /* seconds */

    nvs_get_u32(handle, NVS_KEY_BOOT_COUNT, &boot_count);
    nvs_get_i64(handle, NVS_KEY_BOOT_TIME, &last_boot_time);

    int64_t elapsed = now - last_boot_time;

    if (last_boot_time == 0 || elapsed > RESET_WINDOW_SEC) {
        boot_count = 1;
        ESP_LOGW(TAG, "First boot or window expired (elapsed=%lld s), count=1", elapsed);
    } else {
        boot_count++;
        ESP_LOGW(TAG, "Rapid boot detected (elapsed=%lld s), count=%lu", elapsed, boot_count);
    }

    nvs_set_u32(handle, NVS_KEY_BOOT_COUNT, boot_count);
    nvs_set_i64(handle, NVS_KEY_BOOT_TIME, now);
    nvs_commit(handle);

    if (boot_count >= RESET_THRESHOLD) {
        ESP_LOGW(TAG, ">>> FACTORY RESET TRIGGERED (boot_count=%lu) <<<", boot_count);
        /* Clear boot tracking keys first */
        nvs_set_u32(handle, NVS_KEY_BOOT_COUNT, 0);
        nvs_set_i64(handle, NVS_KEY_BOOT_TIME, 0);
        nvs_commit(handle);
        nvs_close(handle);

        /* Erase all NVS partitions */
        nvs_flash_erase();
        return true;
    }

    nvs_close(handle);
    return false;
}

void factory_reset_confirm_boot(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(reset_nvs, NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    /* Boot survived >30s — considered successful, reset count */
    nvs_set_u32(handle, NVS_KEY_BOOT_COUNT, 0);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Boot confirmed successful, reset counter cleared");
}
