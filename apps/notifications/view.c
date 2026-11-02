#include "view.h"
#include "lvgl.h"
#include "lv_page.h"
#include "lv_controls.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static lv_obj_t *root;
static lv_timer_t *timer;
static bepio_notifications_controller_t *controller;
static uint32_t rendered_revision;
#define SWIPE_DISTANCE 48
#define SWIPE_VERTICAL_LIMIT 24
#define TAP_MOVEMENT_LIMIT 12
static struct {
	uint32_t id;
	lv_point_t start;
	int max_y;
	bool active, suppress_click, vertical;
} swipe;


static void open_item(lv_event_t *event)
{
	uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
	if (swipe.id == id && swipe.suppress_click) return;
	if (!bepio_notifications_controller_select(controller, id)) bepio_notifications_view_refresh();
}
static void remove_item(lv_event_t *event)
{
	(void)event;
	if (!bepio_notifications_controller_delete(controller, controller->selected)) {
		controller->selected = 0;
		bepio_notifications_view_refresh();
	}
}

/* Track displacement rather than gesture velocity, so slow swipes also work.
 * Polling defers rebuilding cards until the current pointer interaction ends. */
static void card_input(lv_event_t *event)
{
	uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
	lv_event_code_t code = lv_event_get_code(event);
	if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_DELETE) {
		if (swipe.id == id) { swipe.active = false; swipe.suppress_click = true; }
		return;
	}
	if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) return;
	lv_indev_t *input = lv_event_get_indev(event);
	if (!input) return;
	lv_point_t point;
	lv_indev_get_point(input, &point);
	if (code == LV_EVENT_PRESSED) {
		swipe.id = id;
		swipe.start = point;
		swipe.max_y = 0;
		swipe.active = true;
		swipe.suppress_click = false;
		swipe.vertical = false;
		return;
	}
	if (!swipe.active || swipe.id != id) return;
	int dx = point.x - swipe.start.x, dy = abs(point.y - swipe.start.y);
	if (dy > swipe.max_y) swipe.max_y = dy;
	if (abs(dx) >= TAP_MOVEMENT_LIMIT || dy >= TAP_MOVEMENT_LIMIT) swipe.suppress_click = true;
	if (dy >= TAP_MOVEMENT_LIMIT && dy >= abs(dx)) swipe.vertical = true;
	if (code != LV_EVENT_RELEASED) return;
	swipe.active = false;
	if (swipe.vertical || swipe.max_y > SWIPE_VERTICAL_LIMIT ||
	    abs(dx) < SWIPE_DISTANCE || abs(dx) < 2 * swipe.max_y) return;
	int ret = dx < 0 ? bepio_notifications_controller_delete(controller, id) :
		bepio_notifications_controller_mark_read(controller, id);
	/* Refresh on the next UI turn, after LVGL finishes release/click dispatch. */
	if (!ret && timer) lv_timer_ready(timer);
}

/* A small right triangle at the card's top-left is the unread indicator. */
static void draw_unread(lv_event_t *event)
{
	lv_area_t area;
	lv_obj_get_coords(lv_event_get_target_obj(event), &area);
	lv_draw_triangle_dsc_t triangle;
	lv_draw_triangle_dsc_init(&triangle);
	triangle.color = lv_color_hex(0x405d43);
	triangle.opa = LV_OPA_COVER;
	triangle.p[0] = (lv_point_precise_t){area.x1 + 2, area.y1 + 2};
	triangle.p[1] = (lv_point_precise_t){area.x1 + 14, area.y1 + 2};
	triangle.p[2] = (lv_point_precise_t){area.x1 + 2, area.y1 + 14};
	lv_draw_triangle(lv_event_get_layer(event), &triangle);
}

void bepio_notifications_view_refresh(void)
{
	if (!root || !controller) return;
	uint32_t sampled_revision = bepio_notifications_revision();
	lv_obj_clean(root);
	lv_obj_scroll_to_y(root, 0, LV_ANIM_OFF);
	bepio_notification_t item;
	if (controller->selected && !bepio_notifications_get(controller->selected, true, &item)) {
		lv_page_set_title(root, "Notification", 0);
		lv_obj_t *app = bepio_ui_label(root, item.payload.app, 12, 8, 296);
		lv_label_set_long_mode(app, LV_LABEL_LONG_DOT);
		lv_obj_set_height(app, 20);
		lv_obj_t *title = bepio_ui_label(root, item.payload.title, 12, 32, 296);
		lv_obj_update_layout(title);
		int y = 32 + lv_obj_get_height(title) + 8;
		lv_obj_t *body = bepio_ui_label(root, item.payload.text, 12, y, 296);
		lv_obj_update_layout(body);
		y += lv_obj_get_height(body) + 12;
		char metadata[128];
		snprintf(metadata, sizeof(metadata), "From: %s\n%s", item.payload.from, item.payload.datetime);
		lv_obj_t *meta = bepio_ui_label(root, metadata, 12, y, 296);
		lv_obj_update_layout(meta);
		y += lv_obj_get_height(meta) + 12;
		bepio_ui_button(root, "Delete", 12, y, 100, 32, remove_item, NULL);
	} else {
		controller->selected = 0;
		lv_page_set_title(root, "Notifications", 0);
		size_t i = 0;
		while (!bepio_notifications_get_at(i, &item)) {
			lv_obj_t *row = lv_button_create(root);
			lv_obj_set_size(row, lv_pct(96), 68);
			lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 8 + (int)i * 76);
			lv_obj_set_style_pad_all(row, 6, 0);
			lv_obj_set_style_pad_left(row, 18, 0);
			lv_obj_set_style_border_width(row, 0, 0);
			lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
			lv_obj_add_flag(row, LV_OBJ_FLAG_PRESS_LOCK);
			lv_obj_set_style_shadow_width(row, 0, 0);
			lv_obj_set_style_radius(row, 6, 0);
			lv_obj_set_style_bg_color(row, lv_color_hex(0xe3ebd4), 0);
			lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
			lv_obj_add_event_cb(row, card_input, LV_EVENT_ALL, (void *)(uintptr_t)item.id);
			lv_obj_add_event_cb(row, open_item, LV_EVENT_CLICKED, (void *)(uintptr_t)item.id);
			if (!item.read) lv_obj_add_event_cb(row, draw_unread, LV_EVENT_DRAW_MAIN, NULL);
			lv_obj_t *app = bepio_ui_label(row, item.payload.app, 0, 0, lv_pct(100));
			lv_label_set_long_mode(app, LV_LABEL_LONG_DOT);
			lv_obj_t *summary = bepio_ui_label(row, item.payload.title[0] ?
				item.payload.title : item.payload.text, 0, 23, lv_pct(100));
			lv_label_set_long_mode(summary, LV_LABEL_LONG_DOT);
			lv_obj_set_height(summary, 26);
			i++;
		}
		if (!i) bepio_ui_label(root, "No notifications", 12, 12, 296);
	}
	rendered_revision = sampled_revision;
}

static void poll(lv_timer_t *unused)
{
	(void)unused;
	if (!swipe.active && rendered_revision != bepio_notifications_revision()) bepio_notifications_view_refresh();
}
int bepio_notifications_view_open(void *parent, bepio_notifications_controller_t *value)
{
	if (!parent || !value) return -EINVAL;
	swipe.active = false;
	swipe.suppress_click = false;
	swipe.id = 0;
	root = parent;
	controller = value;
	lv_obj_set_layout(root, LV_LAYOUT_NONE);
	lv_obj_set_style_pad_all(root, 0, 0);
	lv_obj_set_scroll_dir(root, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_AUTO);
	lv_obj_scroll_to_x(root, 0, LV_ANIM_OFF);
	timer = lv_timer_create(poll, 500, NULL);
	if (!timer) { root = NULL; controller = NULL; return -ENOMEM; }
	bepio_notifications_view_refresh();
	return 0;
}
void bepio_notifications_view_close(void)
{
	swipe.active = false;
	swipe.suppress_click = false;
	swipe.id = 0;
	if (timer) lv_timer_delete(timer);
	timer = NULL;
	root = NULL;
	controller = NULL;
}
