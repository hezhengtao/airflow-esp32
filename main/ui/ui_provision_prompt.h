#pragma once
#include "lvgl.h"

lv_obj_t *ui_provision_prompt_create(lv_obj_t *parent);
void ui_provision_prompt_dismiss(void);       /* safe to call from any task */
void ui_provision_prompt_dismiss_inner(void); /* LVGL task context only */
void ui_provision_prompt_lang_update(void);
