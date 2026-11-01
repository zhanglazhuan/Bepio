#ifndef BEPIO_SETTINGS_PAGE_CONTEXT_H_
#define BEPIO_SETTINGS_PAGE_CONTEXT_H_

#include "../view.h"
#include "lvgl.h"
#include "lv_page.h"
#include <stddef.h>

/* Every page embeds this header as its first member and owns the rest of its state. */
struct settings_page_context {
	settings_view_t *view;
	const settings_model_t *model;
	lv_obj_t *root;
	lv_timer_t *timer;
	void (*refresh)(settings_page_context_t *ctx);
	uint32_t refresh_period_ms;
};

settings_page_context_t *bepio_settings_page_alloc(size_t size, settings_view_t *view,
	const settings_model_t *model);
void bepio_settings_page_back(lv_event_t *event);
void bepio_settings_page_prepare(settings_view_t *view, Page page,
	const char *title);
lv_obj_t *bepio_settings_page_finish(settings_view_t *view, Page page,
	settings_page_context_t *ctx);

#endif
