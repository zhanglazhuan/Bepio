#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "apps/contacts/app.h"
#include "bepio_app_manager.h"
#include "lv_page.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"

static uint16_t frame[240 * 320], draw[320 * 16];
static lv_display_t *display;
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
	bepio_app_manager_exit_app();
}
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
static lv_obj_t *find_text(lv_obj_t *parent, const char *value)
{
	if (lv_obj_check_type(parent, &lv_label_class) && !strcmp(lv_label_get_text(parent), value)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_text(lv_obj_get_child(parent, i), value);
		if (found) { return found; }
	}
	return NULL;
}
static lv_obj_t *find_image(lv_obj_t *parent, const void *source)
{
	if (lv_obj_check_type(parent, &lv_image_class) && lv_image_get_src(parent) == source) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_image(lv_obj_get_child(parent, i), source);
		if (found) { return found; }
	}
	return NULL;
}
static lv_obj_t *find_field(lv_obj_t *parent, const char *placeholder)
{
	if (lv_obj_check_type(parent, &lv_textarea_class) &&
	    !strcmp(lv_textarea_get_placeholder_text(parent), placeholder)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_field(lv_obj_get_child(parent, i), placeholder);
		if (found) { return found; }
	}
	return NULL;
}
static lv_obj_t *find_keyboard(lv_obj_t *parent)
{
	if (lv_obj_check_type(parent, &lv_keyboard_class)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_keyboard(lv_obj_get_child(parent, i));
		if (found) { return found; }
	}
	return NULL;
}
static void key(lv_obj_t *keyboard, const char *value)
{
	for (uint32_t i = 0; lv_buttonmatrix_get_button_text(keyboard, i) != NULL; i++) {
		if (!strcmp(lv_buttonmatrix_get_button_text(keyboard, i), value)) {
			lv_buttonmatrix_set_selected_button(keyboard, i);
			lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, NULL);
			return;
		}
	}
	assert(!"Keyboard key missing");
}
static void click(const char *value)
{
	lv_obj_t *label = find_text(lv_screen_active(), value); assert(label);
	lv_obj_t *button = lv_obj_get_parent(label);
	while (button && !lv_obj_check_type(button, &lv_button_class)) { button = lv_obj_get_parent(button); }
	assert(button); lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
	lv_timer_handler();
	lv_refr_now(display);
}
static void input(const char *field, const char *value)
{
	lv_obj_t *obj = find_field(root, field); assert(obj);
	lv_textarea_set_text(obj, value);
}
static int message_requests;
static int capture_message(const char *phone, const char *name)
{
	assert(!strcmp(phone, "12025550101"));
	assert(!strcmp(name, "Ava Chen"));
	message_requests++;
	return 0;
}
int main(int argc, char **argv)
{
	assert(argc == 2); out_dir = argv[1];
	lv_init(); display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_status_bar_init();
	Page initial = lv_page_create("Contacts", true, page_back, NULL);
	lv_screen_load(initial.screen);
	assert(bepio_contacts_register() == 0);
	contacts_controller_t *c = contacts_app_get_controller();
	assert(c->model->count == 0);
	assert(bepio_app_manager_show(NULL, root, NULL, "Contacts") == 0);
	assert(find_text(root, "No contact yet, click + goto add"));
	assert(find_text(active_page().header_right, LV_SYMBOL_PLUS));
	for (unsigned i = 0; i < 8; i++) {
		char name[24], phone[24];
		uint32_t id;
		snprintf(name, sizeof(name), i == 0 ? "Ava Chen" : "Person %u", i);
		snprintf(phone, sizeof(phone), "120255501%02u", i + 1);
		assert(contacts_model_add(c->model, name, phone, &id) == 0);
	}
	assert(bepio_app_manager_exit_app() == 0);
	assert(bepio_app_manager_show(NULL, root, NULL, "Contacts") == 0);
	contacts_controller_set_compose_handler(c, capture_message);
	assert(c->model->count == 8); assert(find_text(active_page().header, "Contacts")); screenshot("contacts-list");
	assert(!find_field(root, "Search name / phone"));
	assert(!find_text(root, "8 contacts  /  DEMO"));
	lv_obj_t *plus = find_text(active_page().header_right, LV_SYMBOL_PLUS); assert(plus);
	lv_area_t plus_bounds, back_bounds;
	lv_obj_get_coords(plus, &plus_bounds);
	lv_obj_get_coords(lv_obj_get_child(active_page().header, 0), &back_bounds);
	assert(plus_bounds.y1 >= back_bounds.y1 && plus_bounds.y2 <= back_bounds.y2);
	click("Ava Chen"); assert(c->model->flow.page == CONTACTS_PAGE_DETAIL); assert(find_text(lv_screen_active(), "Contact Detail")); assert(!find_text(active_page().header_right, "Send Message"));
	uint32_t favorite_id = c->model->flow.selected_id;
	lv_obj_t *favorite = find_image(root, &ic_bookmark_star_28); assert(favorite);
	lv_obj_send_event(lv_obj_get_parent(favorite), LV_EVENT_CLICKED, NULL);
	contacts_entry_t favorite_entry;
	assert(contacts_controller_lookup(c, favorite_id, &favorite_entry) == 0 && favorite_entry.favorite);
	screenshot("contacts-detail-favorite");
	assert(contacts_controller_back(c));
	assert(find_image(root, &ic_bookmark_star_28)); screenshot("contacts-list-favorite");
	click("Ava Chen");
	favorite = find_image(root, &ic_bookmark_star_28); assert(favorite);
	lv_obj_send_event(lv_obj_get_parent(favorite), LV_EVENT_CLICKED, NULL);
	assert(contacts_controller_lookup(c, favorite_id, &favorite_entry) == 0 && !favorite_entry.favorite);
	assert(contacts_controller_back(c)); assert(!find_image(root, &ic_bookmark_star_28));
	click("Ava Chen");
	lv_obj_t *send = NULL;
	for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
		lv_obj_t *child = lv_obj_get_child(root, i);
		if (!lv_obj_check_type(child, &lv_button_class)) { continue; }
		lv_obj_t *icon = lv_obj_get_child(child, 0);
		if (icon && lv_obj_check_type(icon, &lv_image_class) && lv_image_get_src(icon) == &ic_send_28) { send = child; break; }
	}
	assert(send);
	lv_obj_t *edit = lv_obj_get_parent(find_text(root, LV_SYMBOL_EDIT));
	lv_obj_t *del = lv_obj_get_parent(find_text(root, LV_SYMBOL_TRASH));
	assert(lv_obj_get_width(send) == 148);
	assert(lv_obj_get_x(del) + lv_obj_get_width(del) < lv_obj_get_x(edit));
	assert(lv_obj_get_x(send) >= lv_obj_get_x(edit) + lv_obj_get_width(edit) + 12);
	lv_obj_t *favorite_button = lv_obj_get_parent(find_image(root, &ic_bookmark_star_28));
	assert(lv_obj_get_x(favorite_button) > lv_obj_get_x(edit) + lv_obj_get_width(edit));
	assert(lv_obj_get_x(send) >= lv_obj_get_x(favorite_button) + lv_obj_get_width(favorite_button) + 12);
	lv_obj_t *send_text = find_text(send, "Send Message"); assert(send_text);
	assert(lv_obj_get_height(send_text) == lv_font_get_line_height(lv_obj_get_style_text_font(send_text, 0)));
	assert(lv_obj_get_x(send) + lv_obj_get_width(send) == lv_obj_get_width(root) - 12);
	assert(lv_obj_get_y(send) == lv_obj_get_y(edit) && lv_obj_get_y(send) == lv_obj_get_y(del)); screenshot("contacts-detail");
	lv_obj_send_event(send, LV_EVENT_CLICKED, NULL); assert(message_requests == 1);
	click(LV_SYMBOL_EDIT); assert(c->model->flow.page == CONTACTS_PAGE_EDIT);
	lv_refr_now(display);
	lv_obj_t *kb = find_keyboard(root); assert(kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN));
	lv_area_t bounds; lv_obj_get_coords(kb, &bounds);
	assert(bounds.y1 >= LV_STATUS_BAR_HEIGHT + LV_PAGE_HEADER_HEIGHT &&
	       bounds.y2 < 240);
	assert(find_text(active_page().header, "Edit Contact")); screenshot("contacts-edit");
	lv_obj_t *phone = find_field(root, "Device ID / phone");
	lv_obj_send_event(phone, LV_EVENT_FOCUSED, NULL);
	lv_textarea_set_text(phone, ""); key(kb, "1");
	assert(!strcmp(lv_textarea_get_text(phone), "1"));
	lv_textarea_set_text(phone, "100000000000002");
	assert(!strcmp(lv_textarea_get_text(phone), "100000000000002"));
	screenshot("contacts-device-id");
	input("Name", "Ava Updated"); input("Device ID / phone", "12025550142");
	click("Save"); assert(c->model->flow.page == CONTACTS_PAGE_DETAIL && !strcmp(c->model->entries[0].name, "Ava Updated"));
	click(LV_SYMBOL_EDIT); input("Device ID / phone", c->model->entries[1].phone); click("Save");
	assert(c->model->flow.page == CONTACTS_PAGE_EDIT && find_text(root, "Number already exists"));
	screenshot("contacts-error"); click("Cancel");
	assert(c->model->flow.page == CONTACTS_PAGE_EDIT && c->model->flow.discard_pending);
	assert(find_text(active_page().header, "Edit Contact"));
	assert(find_text(lv_screen_active(), "Discard changes?"));
	screenshot("contacts-discard"); click("Keep editing");
	assert(c->model->flow.page == CONTACTS_PAGE_EDIT && !c->model->flow.discard_pending);
	assert(!find_text(lv_screen_active(), "Discard changes?"));
	assert(!strcmp(lv_textarea_get_text(find_field(root, "Device ID / phone")), c->model->entries[1].phone));
	lv_obj_send_event(lv_obj_get_child(active_page().header, 0), LV_EVENT_CLICKED, NULL);
	assert(c->model->flow.discard_pending && c->model->flow.page == CONTACTS_PAGE_EDIT);
	lv_obj_t *discard_header = find_text(lv_screen_active(), "Discard changes?"); assert(discard_header);
	lv_obj_t *discard_overlay = lv_obj_get_parent(lv_obj_get_parent(lv_obj_get_parent(discard_header)));
	lv_obj_send_event(discard_overlay, LV_EVENT_CLICKED, NULL);
	lv_timer_handler();
	assert(!c->model->flow.discard_pending && c->model->flow.page == CONTACTS_PAGE_EDIT);
	click("Cancel"); click("Discard changes"); assert(c->model->flow.page == CONTACTS_PAGE_DETAIL);
	lv_obj_send_event(lv_obj_get_child(active_page().header, 0), LV_EVENT_CLICKED, NULL); click(LV_SYMBOL_PLUS); assert(find_text(active_page().header, "New Contact")); screenshot("contacts-new");
	input("Name", "Test Person"); input("Device ID / phone", "+"); click("Save");
	assert(c->model->flow.page == CONTACTS_PAGE_EDIT && c->model->count == 8);
	input("Device ID / phone", "100000000000002"); click("Save");
	assert(c->model->flow.page == CONTACTS_PAGE_DETAIL && c->model->count == 9);
	click(LV_SYMBOL_TRASH);
	assert(c->model->flow.page == CONTACTS_PAGE_DETAIL && c->model->flow.delete_pending);
	assert(find_text(active_page().header, "Contact Detail"));
	assert(find_text(lv_screen_active(), "Delete contact?"));
	lv_obj_t *sheet_panel = lv_obj_get_parent(lv_obj_get_parent(find_text(lv_screen_active(), "Delete contact?")));
	lv_obj_update_layout(lv_screen_active());
	lv_area_t sheet_bounds; lv_obj_get_coords(sheet_panel, &sheet_bounds);
	assert(sheet_bounds.y2 == 239 && sheet_bounds.y1 > 0);
	lv_area_t keep_bounds, delete_bounds;
	lv_obj_get_coords(lv_obj_get_parent(find_text(sheet_panel, "Keep contact")), &keep_bounds);
	lv_obj_get_coords(lv_obj_get_parent(find_text(sheet_panel, "Delete contact")), &delete_bounds);
	assert(keep_bounds.y1 == delete_bounds.y1 && keep_bounds.y2 == delete_bounds.y2);
	assert(keep_bounds.x2 < delete_bounds.x1);
	assert(keep_bounds.x1 >= sheet_bounds.x1 && delete_bounds.x2 <= sheet_bounds.x2);
	screenshot("contacts-delete");
	click("Keep contact");
	assert(c->model->count == 9 && !c->model->flow.delete_pending);
	assert(c->model->flow.page == CONTACTS_PAGE_DETAIL);
	assert(!find_text(lv_screen_active(), "Delete contact?"));
	click(LV_SYMBOL_TRASH);
	sheet_panel = lv_obj_get_parent(lv_obj_get_parent(find_text(lv_screen_active(), "Delete contact?")));
	lv_obj_send_event(lv_obj_get_parent(sheet_panel), LV_EVENT_CLICKED, NULL);
	lv_timer_handler();
	assert(!c->model->flow.delete_pending && c->model->flow.page == CONTACTS_PAGE_DETAIL);
	assert(!find_text(lv_screen_active(), "Delete contact?"));
	click(LV_SYMBOL_TRASH); click("Delete contact"); assert(c->model->count == 8 && c->model->flow.page == CONTACTS_PAGE_LIST);
	assert(!find_text(active_page().header_right, "Search name / phone"));
	assert(bepio_app_manager_delete() == 0);
	assert(bepio_app_manager_show(NULL, root, NULL, "Contacts") == 0);
	assert(c->model->count == 8 && !strcmp(c->model->entries[0].name, "Ava Updated"));
	for (unsigned i = 1; i <= 13; i++) {
		char name[20], phone[25]; uint32_t id;
		snprintf(name, sizeof(name), "Zed %02u", i);
		snprintf(phone, sizeof(phone), "120255502%02u", i);
		assert(contacts_controller_add(c, name, phone, &id) == 0);
		lv_timer_handler();
	}
	size_t count; assert(contacts_controller_list(c, &count) == 0 && count == 21);
	click("More"); assert(find_text(root, "Zed 05") && !find_text(root, "Ava Updated"));
	click("Previous"); assert(find_text(root, "Ava Updated"));
	assert(bepio_app_manager_delete() == 0);
	puts("PASS: actual Contacts GUI header add/create/update/validation/discard/delete/reopen");
	return 0;
}
