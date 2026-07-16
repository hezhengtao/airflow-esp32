#include "sensor_history.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "history";

static sensor_sample_t *g_ring = NULL;
static int g_head = 0;
static int g_count = 0;
static SemaphoreHandle_t g_mutex = NULL;

void sensor_history_init(void)
{
    if (g_mutex) return;  /* already initialised */

    g_ring = heap_caps_malloc(sizeof(sensor_sample_t) * SENSOR_HISTORY_MAX_SAMPLES,
                              MALLOC_CAP_SPIRAM);
    if (!g_ring) {
        /* fallback to internal DRAM */
        g_ring = malloc(sizeof(sensor_sample_t) * SENSOR_HISTORY_MAX_SAMPLES);
    }
    if (!g_ring) {
        ESP_LOGE(TAG, "Failed to allocate ring buffer");
        return;
    }

    g_mutex = xSemaphoreCreateMutex();
    if (!g_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(g_ring);
        g_ring = NULL;
        return;
    }
    g_head = 0;
    g_count = 0;
    memset(g_ring, 0, sizeof(sensor_sample_t) * SENSOR_HISTORY_MAX_SAMPLES);
    ESP_LOGI(TAG, "ring buffer ready (%d samples, %u bytes)",
             SENSOR_HISTORY_MAX_SAMPLES,
             (unsigned)(sizeof(sensor_sample_t) * SENSOR_HISTORY_MAX_SAMPLES));
}

void sensor_history_add(time_t ts, float temp,
                        uint16_t tvoc, uint16_t co2, uint16_t ch2o)
{
    if (!g_mutex || !g_ring) return;

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    g_ring[g_head].ts = ts;
    g_ring[g_head].temp = temp;
    g_ring[g_head].tvoc_ugm3 = tvoc;
    g_ring[g_head].co2_ppm = co2;
    g_ring[g_head].ch2o_ugm3 = ch2o;

    g_head = (g_head + 1) % SENSOR_HISTORY_MAX_SAMPLES;
    if (g_count < SENSOR_HISTORY_MAX_SAMPLES) g_count++;

    xSemaphoreGive(g_mutex);
}

int sensor_history_query(time_t since, sensor_sample_t *buf, int max_samples)
{
    if (!g_mutex || !g_ring || !buf || max_samples <= 0) return 0;

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return 0;

    int out = 0;

    /* Walk from oldest to newest */
    int start = (g_count < SENSOR_HISTORY_MAX_SAMPLES) ? 0
               : g_head;  /* oldest entry when full */
    for (int i = 0; i < g_count && out < max_samples; i++) {
        int idx = (start + i) % SENSOR_HISTORY_MAX_SAMPLES;
        if (g_ring[idx].ts >= since) {
            buf[out++] = g_ring[idx];
        }
    }

    xSemaphoreGive(g_mutex);
    return out;
}

int sensor_history_count(void)
{
    return g_count;
}
