#pragma once

/* Forward-declare to avoid pulling LVGL as transitive dependency
 * for components (e.g. wifi_prov) that include this header. */
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

lv_obj_t *ui_screen_power_create(lv_obj_t *parent);
void ui_screen_power_init_schedule(void);  /* boot-time: schedule state + timer */
void ui_screen_power_lang_update(void);
void ui_screen_power_theme_update(void);
void ui_screen_power_sync_schedule(void);
bool ui_screen_power_is_schedule_active(void);
void ui_screen_power_publish_schedule(void);
void ui_screen_power_set_web_schedules(const char *json);
