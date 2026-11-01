#include "chats_page.h"
#include "../view.h"
#include "lv_page.h"
#include "lvgl.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct bepio_chats_page {
	lv_obj_t *root, *conversation, *selected_button;
	bepio_chats_controller_t *controller;
} bepio_chats_page_t;

static lv_obj_t *label(lv_obj_t *parent, const char *value, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, value);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

static bool chinese_text(const char *value)
{
	for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
		if (*p >= 0xe4 && *p <= 0xe9) { return true; }
	}
	return false;
}

static void play(lv_event_t *event)
{
	bepio_chats_page_t *page = lv_event_get_user_data(event);
	(void)bepio_chats_controller_play(page->controller,
		(size_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event)));
}

static void render_conversation(bepio_chats_page_t *page)
{
	lv_obj_t *conversation = page->conversation;
	lv_obj_clean(conversation);
	bepio_chats_contact_t contact;
	if (bepio_chats_controller_contact_at(page->controller,
			bepio_chats_controller_selected(page->controller), &contact)) {
		label(conversation, "Select a contact", 0x8c9aa8);
		return;
	}
	label(conversation, contact.name, 0xe9eff8);
	bepio_chats_message_t message;
	if (bepio_chats_controller_message_at(page->controller, 0, &message)) {
		label(conversation, "No messages yet", 0x8c9aa8);
		return;
	}
	for (size_t i = 0; !bepio_chats_controller_message_at(page->controller, i, &message); i++) {
		bool sent = message.outgoing;
		lv_obj_t *row = lv_obj_create(conversation);
		lv_obj_set_width(row, lv_pct(100));
		lv_obj_set_height(row, LV_SIZE_CONTENT);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(row, 0, 0);
		lv_obj_set_style_pad_all(row, 0, 0);
		lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(row, sent ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
			LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
		lv_obj_t *bubble = lv_obj_create(row);
		lv_obj_set_width(bubble, lv_pct(83));
		lv_obj_set_height(bubble, LV_SIZE_CONTENT);
		lv_obj_set_style_bg_color(bubble, lv_color_hex(sent ? 0x3d7657 : 0x334257), 0);
		lv_obj_set_style_border_width(bubble, 0, 0);
		lv_obj_set_style_radius(bubble, 8, 0);
		lv_obj_set_style_pad_all(bubble, 7, 0);
		lv_obj_set_style_pad_row(bubble, 4, 0);
		lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
		if (chinese_text(message.body)) {
			lv_obj_t *button = lv_button_create(bubble);
			lv_obj_set_width(button, lv_pct(100));
			lv_obj_set_style_shadow_width(button, 0, 0);
			lv_obj_set_user_data(button, (void *)(uintptr_t)i);
			lv_obj_add_event_cb(button, play, LV_EVENT_CLICKED, page);
			label(button, LV_SYMBOL_PLAY "  Voice", 0xffffff);
		} else {
			lv_obj_t *body = label(bubble, message.body[0] ? message.body : "Draft", 0xffffff);
			lv_obj_set_width(body, lv_pct(100));
		}
		if (message.demo) { label(bubble, "Demo", 0xb7c6dc); }
	}
}

static void select_contact(lv_event_t *event)
{
	bepio_chats_page_t *page = lv_event_get_user_data(event);
	if (bepio_chats_controller_select(page->controller,
			(size_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event)))) { return; }
	if (page->selected_button) { lv_obj_set_style_bg_color(page->selected_button, lv_color_hex(0x202c3b), 0); }
	page->selected_button = lv_event_get_target_obj(event);
	lv_obj_set_style_bg_color(page->selected_button, lv_color_hex(0x3d7657), 0);
	render_conversation(page);
}

lv_obj_t *bepio_chats_page_build(void *app, void *payload)
{
	(void)payload;
	bepio_chats_view_t *view = app;
	bepio_chats_page_t *page = calloc(1, sizeof(*page));
	if (!page) { return NULL; }
	Page shell = lv_page_create("Chats", false, NULL, NULL);
	if (!shell.screen) { free(page); return NULL; }
	page->root = shell.container;
	page->controller = view->controller;
	lv_obj_set_style_pad_all(page->root, 0, 0);
	lv_obj_set_style_bg_color(page->root, lv_color_hex(0x15202d), 0);
	lv_obj_set_flex_flow(page->root, LV_FLEX_FLOW_ROW);
	lv_obj_set_style_pad_column(page->root, 0, 0);
	lv_obj_t *contacts = lv_obj_create(page->root);
	lv_obj_set_size(contacts, lv_pct(30), lv_pct(100));
	lv_obj_set_style_pad_all(contacts, 4, 0);
	lv_obj_set_style_pad_row(contacts, 5, 0);
	lv_obj_set_style_bg_color(contacts, lv_color_hex(0x202c3b), 0);
	lv_obj_set_style_border_width(contacts, 0, 0);
	lv_obj_set_style_radius(contacts, 0, 0);
	lv_obj_set_flex_flow(contacts, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(contacts, LV_DIR_VER);
	page->conversation = lv_obj_create(page->root);
	lv_obj_set_size(page->conversation, lv_pct(70), lv_pct(100));
	lv_obj_set_style_pad_all(page->conversation, 7, 0);
	lv_obj_set_style_pad_row(page->conversation, 7, 0);
	lv_obj_set_style_bg_color(page->conversation, lv_color_hex(0x15202d), 0);
	lv_obj_set_style_border_width(page->conversation, 0, 0);
	lv_obj_set_style_radius(page->conversation, 0, 0);
	lv_obj_set_flex_flow(page->conversation, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(page->conversation, LV_DIR_VER);
	bepio_chats_contact_t contact;
	for (size_t i = 0; !bepio_chats_controller_contact_at(page->controller, i, &contact); i++) {
		lv_obj_t *button = lv_button_create(contacts);
		lv_obj_set_width(button, lv_pct(100));
		lv_obj_set_style_bg_color(button, lv_color_hex(0x202c3b), 0);
		lv_obj_set_style_shadow_width(button, 0, 0);
		lv_obj_set_style_pad_all(button, 4, 0);
		lv_obj_set_user_data(button, (void *)(uintptr_t)i);
		lv_obj_add_event_cb(button, select_contact, LV_EVENT_CLICKED, page);
		lv_obj_t *name = label(button, contact.name, 0xe9eff8);
		lv_obj_set_width(name, lv_pct(100));
		lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
		lv_obj_set_height(name, 18);
		if (i == bepio_chats_controller_selected(page->controller)) {
			page->selected_button = button;
			lv_obj_set_style_bg_color(button, lv_color_hex(0x3d7657), 0);
		}
	}
	if (!page->selected_button) { label(contacts, "No contacts", 0x8c9aa8); }
	render_conversation(page);
	view->page_nav.nav_ctx = page;
	return shell.screen;
}
