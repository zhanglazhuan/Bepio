#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "controller.h"
#include "demo.h"
#include "lv_page.h"
#include "apps/contacts/app.h"
#include "bepio_app_manager.h"

#define PREVIEW_WIDTH 320
#define PREVIEW_HEIGHT 240
static uint16_t frame[PREVIEW_WIDTH * PREVIEW_HEIGHT], draw[PREVIEW_WIDTH * 16];
static messages_controller_t controller;
static messages_model_t model;
static messages_view_t view;
static lv_obj_t *active_root(void)
{
	lv_timer_handler();
	return lv_obj_get_child(lv_screen_active(), 2);
}
static Page active_page(void)
{
	lv_obj_t *screen = lv_screen_active();
	lv_obj_t *header = lv_obj_get_child(screen, 1);
	return (Page){.screen = screen, .header = header,
		.header_right = lv_obj_get_child(header, 2), .container = active_root()};
}
#define root active_root()
static void page_back(lv_event_t *event)
{
	(void)event;
	messages_controller_back(&controller);
}
#ifdef _WIN32
#include "window.h"
#endif
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *pixels)
{
	uint16_t *src = (uint16_t *)pixels;
	for (int y = a->y1; y <= a->y2; y++) {
		memcpy(frame + y * PREVIEW_WIDTH + a->x1, src, (a->x2 - a->x1 + 1) * 2);
		src += a->x2 - a->x1 + 1;
	}
	lv_display_flush_ready(display);
}
static void capture(const char *directory, const char *name)
{
	lv_refr_now(NULL);
	char path[512];
	snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
	FILE *out = fopen(path, "wb"); assert(out);
	fprintf(out, "P6\n%d %d\n255\n", PREVIEW_WIDTH, PREVIEW_HEIGHT);
	for (int i = 0; i < PREVIEW_WIDTH * PREVIEW_HEIGHT; i++) {
		uint16_t p = frame[i];
		fputc(((p >> 11) & 31) * 255 / 31, out);
		fputc(((p >> 5) & 63) * 255 / 63, out);
		fputc((p & 31) * 255 / 31, out);
	}
	fclose(out);
}
static lv_obj_t *find_label(lv_obj_t *parent, const char *value)
{
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *child = lv_obj_get_child(parent, i);
		if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), value)) { return child; }
		lv_obj_t *found = find_label(child, value);
		if (found) { return found; }
	}
	return NULL;
}
static void click(const char *value)
{
	lv_obj_t *label = find_label(lv_screen_active(), value); assert(label);
	lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, NULL);
	lv_timer_handler();
}
static lv_obj_t *find_textarea(lv_obj_t *parent, unsigned *index)
{
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *child = lv_obj_get_child(parent, i);
		if (lv_obj_check_type(child, &lv_textarea_class)) {
			if ((*index)-- == 0) { return child; }
		} else {
			lv_obj_t *found = find_textarea(child, index);
			if (found) { return found; }
		}
	}
	return NULL;
}
static lv_obj_t *textarea(unsigned index)
{
	lv_obj_t *obj = find_textarea(root, &index); assert(obj); return obj;
}

static lv_obj_t *dropdown(void)
{
	for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
		lv_obj_t *obj = lv_obj_get_child(root, i);
		if (lv_obj_check_type(obj, &lv_dropdown_class)) { return obj; }
	}
	assert(0); return NULL;
}
static void select_contact(uint32_t selected)
{
	lv_obj_t *obj = dropdown();
	lv_dropdown_set_selected(obj, selected);
	lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	lv_init();
	lv_display_t *display = lv_display_create(PREVIEW_WIDTH, PREVIEW_HEIGHT);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_status_bar_init();
	Page initial = lv_page_create("Messages", true, page_back, NULL);
	lv_screen_load(initial.screen);
	assert(bepio_contacts_register() == 0);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Contacts") == 0);
	uint32_t contact_id;
	assert(contacts_controller_add(contacts_app_get_controller(), "Alex", "13800138000", &contact_id) == 0);
	assert(contacts_controller_add(contacts_app_get_controller(), "Home", "13900139000", &contact_id) == 0);
	assert(contacts_controller_add(contacts_app_get_controller(), "Test Person", "13600136000", &contact_id) == 0);
	assert(bepio_app_manager_delete() == 0);
	bepio_messaging_init();
	bepio_messages_demo_seed();
	messages_view_init(&view, root, NULL);
	messages_controller_init(&controller, &model, &view);
	if (!strcmp(argv[1], "--interactive")) {
#ifdef _WIN32
		bepio_preview_window(frame, root);
		if (controller.active) { messages_controller_deinit(&controller); }
		return 0;
#else
		return 1;
#endif
	}
	assert(model.flow.page == MESSAGES_PAGE_HOME && model.flow.box == BEPIO_MESSAGE_INBOX);
	assert(find_label(root, "* Bepio Lab"));
	assert(!find_label(root, "STAY IN TOUCH") && !find_label(root, "Offline / stored in memory"));
	assert(!find_label(root, LV_SYMBOL_PLUS));
	assert(find_label(active_page().header_right, LV_SYMBOL_PLUS));
	lv_obj_update_layout(active_page().screen);
	lv_area_t add_bounds;
	lv_obj_get_coords(lv_obj_get_parent(find_label(active_page().header_right, LV_SYMBOL_PLUS)), &add_bounds);
	assert(add_bounds.x2 == PREVIEW_WIDTH - 1 && add_bounds.y2 < LV_STATUS_BAR_HEIGHT + LV_PAGE_HEADER_HEIGHT);
	assert(lv_obj_get_style_bg_opa(lv_obj_get_parent(find_label(active_page().header_right, LV_SYMBOL_PLUS)), 0) == LV_OPA_TRANSP);
	capture(argv[1], "home");
	assert(!find_label(root, "Inbox") && !find_label(root, "Outbox"));
	assert(model.flow.page == MESSAGES_PAGE_HOME && model.flow.box == BEPIO_MESSAGE_INBOX);
	capture(argv[1], "inbox");
	click("Alex");
	assert(model.flow.page == MESSAGES_PAGE_DETAIL);
	assert(find_label(root, LV_SYMBOL_PLAY "  Play voice"));
	capture(argv[1], "chat");
	assert(messages_controller_back(&controller));
	click("* Bepio Lab");
	assert(model.flow.page == MESSAGES_PAGE_DETAIL);
	capture(argv[1], "detail");
	click("Delete message");
	capture(argv[1], "delete");
	click("Cancel");
	bepio_messaging_stats_t stats;
	bepio_messaging_stats(&stats); assert(stats.inbox == 3 && stats.unread == 1);
	click("Delete message"); click("Confirm delete");
	bepio_messaging_stats(&stats); assert(stats.inbox == 2);
	unsigned field_index = 0;
	assert(!find_textarea(root, &field_index));
	assert(!find_label(root, "Go"));
	click(LV_SYMBOL_PLUS);
	assert(model.flow.page == MESSAGES_PAGE_COMPOSE && !find_label(active_page().header_right, LV_SYMBOL_PLUS));
	assert(!model.editor.phone[0] && !model.editor.name[0] && !model.editor.body[0]);
	lv_obj_send_event(lv_obj_get_child(active_page().header, 0), LV_EVENT_CLICKED, NULL);
	assert(model.flow.page == MESSAGES_PAGE_HOME && model.pending == MESSAGES_PENDING_NONE);
	assert(!find_label(root, "UNSAVED MESSAGE"));
	click(LV_SYMBOL_PLUS);
	assert(model.flow.page == MESSAGES_PAGE_COMPOSE);
	assert(lv_dropdown_get_option_count(dropdown()) == 4);
	assert(lv_dropdown_get_selected(dropdown()) == 0);
	assert(lv_obj_has_state(lv_obj_get_parent(find_label(root, "Review & queue")), LV_STATE_DISABLED));
	lv_textarea_set_text(textarea(0), "Hello from the interactive preview!");
	select_contact(3);
	assert(!strcmp(model.editor.phone, "13600136000") && !strcmp(model.editor.name, "Test Person"));
	assert(!strcmp(lv_textarea_get_text(textarea(0)), "Hello from the interactive preview!"));
	assert(!lv_obj_has_state(lv_obj_get_parent(find_label(root, "Review & queue")), LV_STATE_DISABLED));
	unsigned extra_field = 1;
	assert(!find_textarea(root, &extra_field));
	lv_dropdown_open(dropdown());
	capture(argv[1], "recipients");
	lv_dropdown_close(dropdown());
	capture(argv[1], "compose");
	click("Save draft");
	bepio_messaging_stats(&stats); assert(stats.outbox == 3);
	assert(model.flow.page == MESSAGES_PAGE_BOX);
	capture(argv[1], "outbox");
	/* Return to the default inbox, then switch back to the saved draft. */
	assert(messages_controller_home(&controller) == 0);
	assert(messages_controller_box(&controller, BEPIO_MESSAGE_OUTBOX) == 0);
	click("Test Person"); click("Edit draft");
	assert(lv_dropdown_get_selected(dropdown()) == 3);
	lv_textarea_set_text(textarea(0), "Updated message");
	click("Review & queue"); capture(argv[1], "review");
	click("Confirm queue");
	bepio_messaging_stats(&stats); assert(stats.queued == 1 && stats.outbox == 3);
	capture(argv[1], "queued");
	lv_obj_send_event(lv_obj_get_child(active_page().header, 0), LV_EVENT_CLICKED, NULL);
	assert(model.flow.page == MESSAGES_PAGE_BOX);
	lv_obj_send_event(lv_obj_get_child(active_page().header, 0), LV_EVENT_CLICKED, NULL);
	assert(model.flow.page == MESSAGES_PAGE_HOME && model.flow.box == BEPIO_MESSAGE_INBOX);
	assert(!messages_controller_back(&controller));
	assert(find_label(active_page().header_right, LV_SYMBOL_PLUS));
	bepio_message_t remaining;
	while (!bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, 0, &remaining)) {
		assert(bepio_messaging_delete(BEPIO_MESSAGE_INBOX, remaining.id) == 0);
	}
	assert(messages_controller_home(&controller) == 0);
	assert(find_label(root, "No messages yet"));
	assert(find_label(active_page().header_right, LV_SYMBOL_PLUS));
	capture(argv[1], "empty");
	/* Unknown recipients in replies/old drafts cannot bypass the selector. */
	assert(messages_controller_new(&controller, "13500135000", "Unknown") == 0);
	assert(lv_dropdown_get_selected(dropdown()) == 0);
	assert(lv_obj_has_state(lv_obj_get_parent(find_label(root, "Review & queue")), LV_STATE_DISABLED));
	assert(messages_controller_discard(&controller) == 0);
	/* Simulate deleting contacts while Messages is closed, then reopen compose. */
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Contacts") == 0);
	contacts_entry_t contact;
	while (!contacts_controller_lookup_at(contacts_app_get_controller(), 0, &contact)) {
		assert(contacts_controller_delete(contacts_app_get_controller(), contact.id) == 0);
	}
	assert(bepio_app_manager_delete() == 0);
	click(LV_SYMBOL_PLUS);
	assert(lv_obj_has_state(dropdown(), LV_STATE_DISABLED));
	assert(find_label(root, "Add a contact in Contacts first."));
	assert(lv_obj_has_state(lv_obj_get_parent(find_label(root, "Review & queue")), LV_STATE_DISABLED));
	capture(argv[1], "no-contacts");
	messages_controller_deinit(&controller);
	puts("PASS: actual LVGL events: default inbox, header +, no search bar, read, compose, save, edit, queue, delete/cancel");
	return 0;
}
