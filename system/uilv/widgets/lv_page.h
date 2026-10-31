/* Page skeleton adapted from DeepStoa/EPOS. SPDX-License-Identifier: GPL-3.0-only */
#ifndef BEPIO_LV_PAGE_H_
#define BEPIO_LV_PAGE_H_
#include "lvgl.h"
#include "lv_status_bar.h"
#define BEPIO_PAGE_HEADER_HEIGHT 32
#define BEPIO_PAGE_BOTTOM_HEIGHT 16

typedef struct {
	lv_obj_t *screen, *header, *back, *header_right, *container;
} bepio_page_t;
/* Call on the LVGL owner thread. Screen lifetime belongs to the caller. */
bepio_page_t bepio_page_create(const char *title, bool allow_back,
			      lv_event_cb_t back_cb, void *user_data);
typedef int (*bepio_page_message_cb_t)(const char *phone, const char *name);
void bepio_page_set_message_handler(bepio_page_message_cb_t handler);
int bepio_page_compose_message(const char *phone, const char *name);
void bepio_page_set_title(const lv_obj_t *content, const char *title, int right_width);
lv_obj_t *bepio_page_header_right(const lv_obj_t *content);
bool bepio_page_has_header(const lv_obj_t *content);
#endif
