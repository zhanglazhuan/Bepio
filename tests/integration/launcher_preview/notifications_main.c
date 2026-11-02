#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_page.h"
#include "bepio_app_manager.h"
#include "apps/notifications/app.h"
#include "apps/notifications/demo.h"
#include "system/uilv/imgs/icon_40x40/bepio_icons.h"
static uint16_t frame[320 * 240], draw[320 * 16];
static lv_display_t *display;
static const char *out_dir;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels)
{
	uint16_t *src = (uint16_t *)pixels;
	for (int y = a->y1; y <= a->y2; y++) {
		memcpy(frame + y * 320 + a->x1, src, (a->x2 - a->x1 + 1) * 2);
		src += a->x2 - a->x1 + 1;
	}
	lv_display_flush_ready(d);
}
static void screenshot(const char *name)
{
	lv_refr_now(display);
	char path[512]; snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
	FILE *f = fopen(path, "wb"); assert(f);
	fprintf(f, "P6\n320 240\n255\n");
	for (int i = 0; i < 240 * 320; i++) {
		uint16_t p = frame[i];
		fputc(((p >> 11) & 31) * 255 / 31, f);
		fputc(((p >> 5) & 63) * 255 / 63, f);
		fputc((p & 31) * 255 / 31, f);
	}
	fclose(f);
}

static lv_obj_t *find_label(lv_obj_t *parent, const char *text)
{
	if (lv_obj_check_type(parent, &lv_label_class) && !strcmp(lv_label_get_text(parent), text)) return parent;
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_label(lv_obj_get_child(parent, i), text);
		if (found) return found;
	}
	return NULL;
}
static void back(lv_event_t *event)
{
	(void)event;
	assert(!bepio_app_manager_exit_app());
}
static lv_indev_data_t pointer_data;
static void pointer_read(lv_indev_t *input, lv_indev_data_t *data)
{
	(void)input;
	*data = pointer_data;
}
static void sample(lv_indev_t *input, int x, int y, bool pressed)
{
	pointer_data.point = (lv_point_t){x, y};
	pointer_data.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
	lv_tick_inc(20);
	lv_indev_read(input);
}
static void swipe_card(lv_indev_t *input, int x1, int y1, int x2, int y2)
{
	lv_obj_update_layout(lv_screen_active());
	sample(input, x1, y1, true);
	for (int i = 1; i <= 6; i++) sample(input, x1 + (x2 - x1) * i / 6,
		y1 + (y2 - y1) * i / 6, true);
	sample(input, x2, y2, false);
	lv_timer_handler();
}

int main(int argc, char **argv)
{
	assert(argc == 2); out_dir = argv[1];
	lv_init(); display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_status_bar_init();
	Page page = lv_page_create("Notifications", true, back, NULL);
	lv_screen_load(page.screen);
	bepio_notifications_init();
	assert(!bepio_notifications_register());
	assert(bepio_app_manager_get_app(0)->icon == &ic_notifications);
	assert(ic_notifications.header.w == 40 && ic_notifications.header.h == 40);
	assert(!bepio_app_manager_show(NULL, page.container, NULL, "Notifications"));
	assert(find_label(page.container, "No notifications"));
	screenshot("notifications-empty");
	assert(bepio_notifications_demo_seed() == 4);
	lv_tick_inc(600); lv_timer_handler();
	assert(find_label(page.container, "System"));
	assert(find_label(page.container, "WeChat"));
	assert(find_label(page.container, "Calendar"));
	assert(find_label(page.container, "Mail"));
	screenshot("notifications-demo");
	bepio_notification_payload_t p = {.id = "fixture-chat", .from = "101", .app = "Chat",
		.title = "Ada", .text = "See you at 3 pm. Bring your pager!", .datetime = "2026-11-02T01:00:00Z"};
	assert(!bepio_notifications_receive(&p));
	strcpy(p.id, "fixture-mail"); strcpy(p.app, "Mail"); strcpy(p.title, "Live weekly update");
	strcpy(p.text, "Your project summary is ready.");
	assert(!bepio_notifications_receive(&p));
	lv_tick_inc(600); lv_timer_handler();
	assert(find_label(page.container, "Mail"));
	assert(find_label(page.container, "Chat"));
	screenshot("notifications-list");
	assert(lv_obj_get_scroll_dir(page.container) == LV_DIR_VER);
	assert(lv_obj_get_scroll_left(page.container) == 0);
	assert(lv_obj_get_scroll_right(page.container) <= 0);
	lv_obj_t *card = lv_obj_get_parent(find_label(page.container, "Live weekly update"));
	int width = lv_obj_get_width(card), parent_width = lv_obj_get_content_width(page.container);
	assert(width * 100 >= parent_width * 95 && width * 100 <= parent_width * 98);
	assert(!lv_obj_has_flag(card, LV_OBJ_FLAG_SCROLLABLE));
	lv_area_t unread_area;
	lv_obj_get_coords(card, &unread_area);
	uint16_t unread_pixel = frame[(unread_area.y1 + 4) * 320 + unread_area.x1 + 4];
	assert(unread_pixel != frame[(unread_area.y1 + 20) * 320 + unread_area.x1 + 4]);
	lv_obj_t *row = lv_obj_get_parent(find_label(page.container, "Ada"));
	lv_obj_send_event(row, LV_EVENT_CLICKED, NULL);
	assert(find_label(page.container, "See you at 3 pm. Bring your pager!"));
	screenshot("notifications-detail");
	assert(!bepio_app_manager_exit_app());
	assert(find_label(page.container, "Chat"));
	screenshot("notifications-read");
	card = lv_obj_get_parent(find_label(page.container, "Ada"));
	lv_area_t read_area;
	lv_obj_get_coords(card, &read_area);
	assert(frame[(read_area.y1 + 4) * 320 + read_area.x1 + 4] != unread_pixel);
	assert(lv_obj_get_scroll_right(page.container) <= 0);
	assert(!bepio_app_manager_exit_app());
	assert(!bepio_app_manager_is_app_running());
	assert(!bepio_app_manager_show(NULL, page.container, NULL, "Notifications"));
	assert(find_label(page.container, "Live weekly update"));
	row = lv_obj_get_parent(find_label(page.container, "Live weekly update"));
	lv_obj_send_event(row, LV_EVENT_CLICKED, NULL);
	lv_obj_t *button = lv_obj_get_parent(find_label(page.container, "Delete"));
	lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
	assert(!find_label(page.container, "Live weekly update"));
	assert(find_label(page.container, "Ada"));
	assert(!bepio_app_manager_delete());
	lv_tick_inc(600); lv_timer_handler(); /* Stopped view has no polling callback. */

	/* Exercise real LVGL pointer dispatch, including slow drags and click suppression. */
	bepio_notifications_init();
	strcpy(p.id, "gesture-one"); strcpy(p.title, "Swipe target");
	assert(!bepio_notifications_receive(&p));
	assert(!bepio_app_manager_show(NULL, page.container, NULL, "Notifications"));
	lv_indev_t *input = lv_indev_create();
	lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(input, pointer_read);
	bepio_notification_t item;
	swipe_card(input, 100, 90, 120, 90); /* Short horizontal movement does not open or act. */
	assert(!bepio_notifications_get_at(0, &item) && !item.read);
	assert(find_label(page.container, "Swipe target"));
	assert(!find_label(page.container, "Delete"));
	swipe_card(input, 100, 90, 190, 135); /* Diagonal motion is not an action. */
	assert(!bepio_notifications_get_at(0, &item) && !item.read);
	assert(!find_label(page.container, "Delete"));
	swipe_card(input, 100, 90, 190, 90); /* Right marks read and stays on list. */
	assert(!bepio_notifications_get_at(0, &item) && item.read);
	assert(find_label(page.container, "Swipe target"));
	assert(!find_label(page.container, "Delete"));
	screenshot("notifications-swipe-read");
	swipe_card(input, 100, 90, 190, 90); /* Repeating right swipe is a no-op. */
	assert(!bepio_notifications_get_at(0, &item) && item.read);
	swipe_card(input, 210, 90, 110, 90); /* Left deletes, without entering detail. */
	assert(bepio_notifications_get_at(0, &item) != 0);
	assert(find_label(page.container, "No notifications"));
	screenshot("notifications-swipe-delete");
	bepio_notifications_init();
	assert(bepio_notifications_demo_seed() == 4);
	lv_tick_inc(600); lv_timer_handler();
	swipe_card(input, 100, 170, 100, 90); /* Vertical scrolling remains available. */
	assert(lv_obj_get_scroll_y(page.container) > 0);
	assert(lv_obj_get_scroll_x(page.container) == 0);
	assert(!find_label(page.container, "Delete"));
	for (size_t i = 0; i < 4; i++) {
		assert(!bepio_notifications_get_at(i, &item) && !item.read);
	}
	bepio_notifications_init();
	assert(!bepio_notifications_receive(&p));
	lv_tick_inc(600); lv_timer_handler();
	sample(input, 100, 90, true); sample(input, 100, 90, false);
	assert(find_label(page.container, "Delete")); /* A normal tap still opens detail. */
	assert(!bepio_app_manager_delete());
	lv_indev_delete(input);
	puts("PASS: Notifications empty/live list, detail/read/back/delete, retained RAM, triangle/96% geometry, pointer swipe read/delete and tap isolation");
}
