#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "event_bus";

#define MAX_SUBSCRIBERS    8
#define QUEUE_DEPTH        32

typedef struct {
    event_handler_t handler;
    void *user_data;
} subscriber_t;

typedef struct {
    subscriber_t subs[MAX_SUBSCRIBERS];
    int count;
} event_channel_t;

static event_channel_t g_channels[EVENT_MAX];
static QueueHandle_t g_queue = NULL;

void event_bus_init(void)
{
    g_queue = xQueueCreate(QUEUE_DEPTH, sizeof(event_t));
    memset(g_channels, 0, sizeof(g_channels));
    ESP_LOGI(TAG, "Initialized (max %d events, %d subs/channel)", EVENT_MAX, MAX_SUBSCRIBERS);
}

bool event_bus_subscribe(event_id_t id, event_handler_t handler, void *user_data)
{
    if (id >= EVENT_MAX || g_channels[id].count >= MAX_SUBSCRIBERS) return false;
    for (int i = 0; i < g_channels[id].count; i++) {
        if (g_channels[id].subs[i].handler == handler) return true; /* already */
    }
    int n = g_channels[id].count++;
    g_channels[id].subs[n].handler = handler;
    g_channels[id].subs[n].user_data = user_data;
    return true;
}

bool event_bus_unsubscribe(event_id_t id, event_handler_t handler)
{
    if (id >= EVENT_MAX) return false;
    for (int i = 0; i < g_channels[id].count; i++) {
        if (g_channels[id].subs[i].handler == handler) {
            g_channels[id].subs[i] = g_channels[id].subs[g_channels[id].count - 1];
            g_channels[id].count--;
            return true;
        }
    }
    return false;
}

void event_bus_publish(const event_t *event)
{
    if (!event || event->id >= EVENT_MAX) return;

    /* Notify subscribers in caller's context (fast handlers only) */
    for (int i = 0; i < g_channels[event->id].count; i++) {
        g_channels[event->id].subs[i].handler(event,
            g_channels[event->id].subs[i].user_data);
    }

    /* Also push to queue for async consumers */
    event_t copy = *event;
    xQueueSend(g_queue, &copy, 0);
}
