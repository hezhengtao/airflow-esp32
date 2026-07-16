#include "holiday_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

#define TAG "holiday"
#define HOLIDAY_URL_BASE "http://timor.tech/api/holiday/year"

/* ── Data structures ───────────────────────────────────────────────── */

#define HOLIDAY_MAX_ENTRIES 50

typedef struct {
    uint8_t month;
    uint8_t day;
    bool    is_rest;      /* true=休, false=调休补班 */
    char    name[16];
} holiday_entry_t;

typedef struct {
    int              year;
    int              count;
    holiday_entry_t  entries[HOLIDAY_MAX_ENTRIES];
    bool             loaded;
} holiday_cache_t;

static holiday_cache_t g_cache;
static SemaphoreHandle_t g_mutex = NULL;

/* ── HTTP infrastructure ────────────────────────────────────────────── */

static char *g_http_buf = NULL;
static int   g_http_len = 0;
static int   g_http_cap = 0;

static esp_err_t http_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGW(TAG, "HTTP error: status=%d",
                 esp_http_client_get_status_code(evt->client));
        break;
    case HTTP_EVENT_ON_DATA: {
        int need = g_http_len + evt->data_len;
        if (need > g_http_cap) {
            int new_cap = g_http_cap ? g_http_cap * 2 : 8192;
            if (new_cap < need) new_cap = need;
            char *new_buf = malloc(new_cap);
            if (!new_buf) {
                ESP_LOGE(TAG, "OOM");
                free(g_http_buf);
                g_http_buf = NULL;
                g_http_len = 0;
                g_http_cap = 0;
                return ESP_FAIL;
            }
            if (g_http_buf) {
                memcpy(new_buf, g_http_buf, g_http_len);
                free(g_http_buf);
            }
            g_http_buf = new_buf;
            g_http_cap = new_cap;
        }
        memcpy(g_http_buf + g_http_len, evt->data, evt->data_len);
        g_http_len += evt->data_len;
        break;
    }
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP finished, len=%d", g_http_len);
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* ── Parse response ─────────────────────────────────────────────────── */

static void parse_response(const char *body, int len)
{
    cJSON *root = cJSON_ParseWithLength(body, len);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || !cJSON_IsNumber(code) || code->valueint != 0) {
        ESP_LOGW(TAG, "API returned error code");
        cJSON_Delete(root);
        return;
    }

    cJSON *holiday_obj = cJSON_GetObjectItem(root, "holiday");
    if (!holiday_obj || !cJSON_IsObject(holiday_obj)) {
        ESP_LOGW(TAG, "Missing 'holiday' object");
        cJSON_Delete(root);
        return;
    }

    /* Determine year from current time (NTP) */
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    int year = ti.tm_year + 1900;

    holiday_cache_t cache;
    memset(&cache, 0, sizeof(cache));
    cache.year = year;
    cache.count = 0;

    /* Iterate all MM-DD keys in the holiday object */
    cJSON *entry = holiday_obj->child;
    while (entry && cache.count < HOLIDAY_MAX_ENTRIES) {
        if (cJSON_IsObject(entry)) {
            const char *key = entry->string; /* "MM-DD" */
            if (key && strlen(key) == 5 && key[2] == '-') {
                cache.entries[cache.count].month = (uint8_t)((key[0]-'0')*10 + (key[1]-'0'));
                cache.entries[cache.count].day   = (uint8_t)((key[3]-'0')*10 + (key[4]-'0'));

                cJSON *hol = cJSON_GetObjectItem(entry, "holiday");
                if (hol && cJSON_IsBool(hol)) {
                    cache.entries[cache.count].is_rest = (bool)hol->valueint;
                } else {
                    cache.entries[cache.count].is_rest = true; /* default rest */
                }

                cJSON *name = cJSON_GetObjectItem(entry, "name");
                if (name && cJSON_IsString(name)) {
                    strncpy(cache.entries[cache.count].name, name->valuestring, 15);
                    cache.entries[cache.count].name[15] = '\0';
                }

                cache.count++;
            }
        }
        entry = entry->next;
    }

    cache.loaded = true;
    cJSON_Delete(root);

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(1000))) {
        g_cache = cache;
        xSemaphoreGive(g_mutex);
    }

    ESP_LOGI(TAG, "Holiday cache loaded: year=%d entries=%d", cache.year, cache.count);
}

/* ── HTTP fetch ─────────────────────────────────────────────────────── */

static void holiday_fetch(int year)
{
    char url[128];
    snprintf(url, sizeof(url), "%s/%d/", HOLIDAY_URL_BASE, year);
    ESP_LOGI(TAG, "Fetching: %s", url);

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_handler,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .user_agent = "AiRFLOW/2.0 ESP32",
        .keep_alive_enable = false,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGW(TAG, "HTTP client init failed");
        return;
    }
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_header(client, "Accept", "application/json");

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK && g_http_buf && g_http_len > 0) {
        parse_response(g_http_buf, g_http_len);
    } else {
        ESP_LOGW(TAG, "HTTP fetch failed: %s", esp_err_to_name(err));
    }

    free(g_http_buf);
    g_http_buf = NULL;
    g_http_len = 0;
    g_http_cap = 0;

    esp_http_client_cleanup(client);
}

/* ── Thread-safe lookup ─────────────────────────────────────────────── */

static const holiday_entry_t *find_entry(int month, int day)
{
    if (!g_mutex || !g_cache.loaded) return NULL;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return NULL;
    for (int i = 0; i < g_cache.count; i++) {
        if (g_cache.entries[i].month == month && g_cache.entries[i].day == day) {
            const holiday_entry_t *e = &g_cache.entries[i];
            xSemaphoreGive(g_mutex);
            return e;
        }
    }
    xSemaphoreGive(g_mutex);
    return NULL;
}

bool holiday_is_workday(int month, int day, int wday)
{
    const holiday_entry_t *e = find_entry(month, day);
    if (e) return !e->is_rest;  /* 调休补班=工作日 */
    return (wday >= 1 && wday <= 5);  /* 回退: 周一~五=工作日 */
}

bool holiday_is_rest_day(int month, int day, int wday)
{
    const holiday_entry_t *e = find_entry(month, day);
    if (e) return e->is_rest;  /* 法定假日=休息日 */
    return (wday == 0 || wday == 6);  /* 回退: 周六日=休息日 */
}

const char *holiday_get_name(int month, int day)
{
    const holiday_entry_t *e = find_entry(month, day);
    if (e && e->is_rest && e->name[0]) return e->name;
    return NULL;
}

/* ── Background task ────────────────────────────────────────────────── */

static void holiday_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(8000));  /* wait for NTP sync after WiFi */

    int fail_count = 0;
    int last_year = 0;
    bool first_fetch = true;

    while (1) {
        time_t now = time(NULL);
        struct tm ti;
        localtime_r(&now, &ti);
        int year = ti.tm_year + 1900;

        if (year < 2025) {
            /* NTP not yet synced, retry soon */
            vTaskDelay(pdMS_TO_TICKS(30000));
            continue;
        }

        /* Fetch if: first run, year changed, or previously failed */
        bool need_fetch = first_fetch || (year != last_year) || !g_cache.loaded;

        if (need_fetch) {
            holiday_fetch(year);
            last_year = year;
            first_fetch = false;

            if (g_cache.loaded) {
                fail_count = 0;
                ESP_LOGI(TAG, "Holiday fetch OK, next check in 24h");
                vTaskDelay(pdMS_TO_TICKS(86400000)); /* 24h */
            } else {
                fail_count++;
                int delay_s = (fail_count <= 3) ? 30 : 3600;
                ESP_LOGW(TAG, "Fetch failed, retry in %ds (#%d)", delay_s, fail_count);
                vTaskDelay(pdMS_TO_TICKS(delay_s * 1000));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(3600000)); /* 1h idle check */
        }
    }
}

void holiday_client_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    memset(&g_cache, 0, sizeof(g_cache));
    xTaskCreate(holiday_task, "holiday", 6144, NULL, 3, NULL);
    ESP_LOGI(TAG, "Holiday task started");
}
