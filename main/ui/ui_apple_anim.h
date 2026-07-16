#pragma once

#include "lvgl.h"

/* ── Apple Design Motion System ────────────────────────────────── */

/* Use LVGL v9 built-in easing which closely matches Apple curves:
 * lv_anim_path_ease_in_out  ≈ cubic-bezier(0.4,0.0,0.2,1.0)
 * lv_anim_path_ease_in      ≈ cubic-bezier(0.0,0.0,0.2,1.0)
 * lv_anim_path_ease_out     ≈ cubic-bezier(0.4,0.0,1.0,1.0)
 */

/* Spring parameters (iOS modal spring) */
#define APPLE_SPRING_STIFFNESS   157.0f
#define APPLE_SPRING_DAMPING     24.0f
#define APPLE_SPRING_MASS        1.0f

/* Press interaction durations */
#define APPLE_PRESS_DURATION_MS  80
#define APPLE_RELEASE_DURATION_MS 250

/* Screen transition durations */
#define APPLE_PUSH_DURATION_MS   350
#define APPLE_POP_DURATION_MS    320

/* ── Public API ───────────────────────────────────────────────── */
void apple_anim_init(void);

/* Apply Apple-style press effect to any object (scale 1.0→0.96, opacity 100→80%) */
void apple_press_effect(lv_obj_t *obj);
void apple_release_effect(lv_obj_t *obj);

/* Convenience callbacks for lv_obj_add_event_cb — extract target from event */
static inline void apple_press_cb(lv_event_t *e) { apple_press_effect(lv_event_get_target(e)); }
static inline void apple_release_cb(lv_event_t *e) { apple_release_effect(lv_event_get_target(e)); }

/* Hierarchical screen transition: slides new in from right for "push",
 * slides old out to right for "pop" (back navigation). Includes fade.
 * 'forward': true = push (go deeper), false = pop (go back) */
void apple_screen_transition(lv_obj_t *from, lv_obj_t *to, bool forward);

/* Spring bounce: object briefly overshoots and settles (for attention) */
void apple_spring_bounce(lv_obj_t *obj);

/* Spring settle for slider/arc — natural inertia feel */
void apple_spring_settle(lv_obj_t *obj, float start, float end);

/* Smooth arc value transition with Apple easing */
void apple_arc_animate(lv_obj_t *arc, int32_t target_value, uint16_t duration_ms);

/* Generic property animation with Apple ease-in-out */
void apple_anim_float(lv_obj_t *obj, float *var, float target, uint16_t duration_ms);
