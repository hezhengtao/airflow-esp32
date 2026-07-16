#pragma once

#include <stdbool.h>

/**
 * Check factory reset condition on boot.
 * Reads boot_count/last_boot_time from NVS.
 * If 3 boots occur within 15 seconds, erases all NVS and returns true.
 * Must be called early in app_main, before settings are loaded.
 */
bool factory_reset_check(void);

/**
 * Call this after app has been running stably for 30+ seconds.
 * Resets the boot counter so subsequent normal boots don't trigger.
 */
void factory_reset_confirm_boot(void);
