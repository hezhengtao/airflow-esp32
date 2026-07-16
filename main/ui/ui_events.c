#include "lvgl.h"
#include "ui_manager.h"
#include "ui_apple_anim.h"

/*
 * Event handlers for interactive widgets are now registered directly
 * in each screen's _create() function:
 *   ui_screen_fan.c  — slider, power toggle
 *   ui_screen_brightness.c — brightness slider
 *   ui_screen_power.c — screen off, shutdown
 *   ui_screen_network.c — connect, provisioning, theme toggle
 *
 * This file is a stub kept for future cross-screen event handling.
 */
