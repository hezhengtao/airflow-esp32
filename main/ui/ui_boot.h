#pragma once
#include "lvgl.h"

/**
 * Create and display the boot animation splash screen on `parent`.
 * Shows logo with fade-in → hold → fade-out, then calls `on_done` callback.
 *
 * @param parent   the active LVGL screen (usually lv_screen_active())
 * @param on_done  callback invoked when animation completes (NULL ok)
 */
void ui_boot_show(lv_obj_t *parent, void (*on_done)(void));
void ui_boot_hide(void);
