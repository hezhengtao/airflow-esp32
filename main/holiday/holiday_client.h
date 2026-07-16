#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start background fetch task. Call after WiFi connects. */
void holiday_client_init(void);

/* Check if given date is a workday (Mon-Fri normal or adjusted 调休补班) */
bool holiday_is_workday(int month, int day, int wday);

/* Check if given date is a rest day (weekend normal or statutory holiday) */
bool holiday_is_rest_day(int month, int day, int wday);

/* Return holiday name if date is a statutory holiday, NULL otherwise */
const char *holiday_get_name(int month, int day);

#ifdef __cplusplus
}
#endif
