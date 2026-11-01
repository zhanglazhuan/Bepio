#ifndef BEPIO_MESSAGES_PAGE_COMMON_H_
#define BEPIO_MESSAGES_PAGE_COMMON_H_
#include "../view.h"
#include "lvgl.h"

lv_obj_t *messages_page_text(lv_obj_t *parent, const char *value, uint32_t color);
lv_obj_t *messages_page_button(messages_page_context_t *ctx, lv_obj_t *parent,
	const char *value, int code, uint32_t color);
void messages_page_result(messages_page_context_t *ctx, int ret);
void messages_page_action(lv_event_t *event);
bool messages_page_has_chinese(const char *value);
#endif
