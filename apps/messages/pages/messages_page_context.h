#ifndef BEPIO_MESSAGES_PAGE_CONTEXT_H_
#define BEPIO_MESSAGES_PAGE_CONTEXT_H_
#include "../controller.h"
#include "lvgl.h"
#include "lv_page.h"
#include <stddef.h>
#include <stdint.h>

struct messages_page_context {
	messages_view_t *view;
	messages_controller_t *controller;
	lv_obj_t *root;
	lv_obj_t *notice, *keyboard;
	int (*sync)(messages_page_context_t *ctx);
};

enum { ACT_NEW = 1, ACT_REPLY, ACT_DELETE,
	ACT_CONFIRM, ACT_CANCEL, ACT_SAVE, ACT_SEND, ACT_DISCARD, ACT_EDIT, ACT_RETRY };

messages_page_context_t *messages_page_alloc(size_t size, messages_view_t *view);
void messages_page_prepare(messages_page_context_t *ctx, Page page, const char *title);
lv_obj_t *messages_page_finish(messages_view_t *view, Page page, messages_page_context_t *ctx);
void messages_page_back(lv_event_t *event);
#endif
