/* Standard page skeleton adapted from DeepStoa. SPDX-License-Identifier: GPL-3.0-only */
#ifndef LV_PAGE_WIDGET_H
#define LV_PAGE_WIDGET_H

#include <lvgl.h>
#include "lv_status_bar.h"

/* Compact LCD header; DeepStoa uses 50px for its larger e-paper type. */
#define LV_PAGE_HEADER_HEIGHT 32
#define LV_PAGE_HOR_PAD 8

typedef struct {
	lv_obj_t *screen;
	lv_obj_t *header;
	lv_obj_t *header_right;
	lv_obj_t *container;
} Page;

/* Build a screen with status-bar space, header and growing content container. */
Page lv_page_create(const char *title, bool allow_back,
	lv_event_cb_t back_cb, void *user_data);

/* Helpers for apps that update a page inside an existing screen. */
void lv_page_set_title(const lv_obj_t *content, const char *title, int right_width);
lv_obj_t *lv_page_header_right(const lv_obj_t *content);
bool lv_page_has_header(const lv_obj_t *content);

#endif
