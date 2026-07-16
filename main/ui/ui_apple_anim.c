#include "ui_apple_anim.h"
#include "lvgl.h"

void apple_anim_init(void) {}

void apple_press_effect(lv_obj_t *obj) { (void)obj; }

void apple_release_effect(lv_obj_t *obj) { (void)obj; }

void apple_screen_transition(lv_obj_t *from, lv_obj_t *to, bool forward)
{
    (void)forward;
    /* Instant switch — no animation */
    lv_obj_add_flag(from, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(to, LV_OBJ_FLAG_HIDDEN);
}

void apple_spring_bounce(lv_obj_t *obj) { (void)obj; }

void apple_spring_settle(lv_obj_t *obj, float start, float end)
{
    (void)obj; (void)start; (void)end;
}

void apple_arc_animate(lv_obj_t *arc, int32_t target_value, uint16_t duration_ms)
{
    (void)duration_ms;
    lv_arc_set_value(arc, target_value);
}

void apple_anim_float(lv_obj_t *obj, float *var, float target, uint16_t duration_ms)
{
    (void)obj; (void)duration_ms;
    *var = target;
}
