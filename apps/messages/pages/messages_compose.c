#include "messages_compose.h"
#include "messages_page_context.h"
#include "messages_page_common.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef CONFIG_BEPIO_APP_CONTACTS
#include "apps/contacts/app.h"
#define RECIPIENT_CAPACITY CONFIG_BEPIO_CONTACTS_CAPACITY
#else
#define RECIPIENT_CAPACITY 1
#endif

typedef struct {
	messages_page_context_t base;
	lv_obj_t *recipient_select, *body_input, *send_button;
#ifdef CONFIG_BEPIO_APP_CONTACTS
	uint32_t recipient_ids[RECIPIENT_CAPACITY];
#endif
	size_t recipient_count;
	char recipient_options[40 + RECIPIENT_CAPACITY * (BEPIO_MESSAGE_NAME_MAX + BEPIO_MESSAGE_PHONE_MAX + 5)];
} compose_context_t;

static int sync_editor(messages_page_context_t *base)
{
	compose_context_t *ctx = (compose_context_t *)base;
	messages_controller_t *controller = base->controller;
	/* Copy selection through public controllers without rebuilding live widgets. */
	char body[BEPIO_MESSAGE_BODY_MAX + 1];
	snprintf(body, sizeof(body), "%s", lv_textarea_get_text(ctx->body_input));
	uint32_t selected = lv_dropdown_get_selected(ctx->recipient_select);
	const char *phone = "", *name = "";
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t contact;
	if (selected && selected <= ctx->recipient_count) {
		int ret = contacts_controller_lookup(contacts_app_get_controller(), ctx->recipient_ids[selected - 1], &contact);
		if (!ret) { phone = contact.phone; name = contact.name; }
		else {
			lv_dropdown_set_selected(ctx->recipient_select, 0);
			selected = 0;
			lv_obj_remove_flag(ctx->base.notice, LV_OBJ_FLAG_HIDDEN);
			lv_label_set_text(ctx->base.notice, "Contact unavailable. Choose another contact.");
		}
	}
#endif
	controller->view->available = false;
	int ret = messages_controller_recipient(controller, phone, name);
	if (!ret) { ret = messages_controller_text(controller, body); }
	controller->view->available = true;
	if (ctx->send_button) {
		if (selected && body[0] && !ret) { lv_obj_remove_state(ctx->send_button, LV_STATE_DISABLED); }
		else { lv_obj_add_state(ctx->send_button, LV_STATE_DISABLED); }
	}
	return ret;
}

static void keep_input_visible(compose_context_t *ctx);

static void edited(lv_event_t *event)
{
	compose_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->recipient_select && ctx->body_input) {
		messages_page_result(&ctx->base, sync_editor(&ctx->base));
		if (lv_event_get_target_obj(event) == ctx->body_input) {
			keep_input_visible(ctx);
		}
	}
}

static void keep_input_visible(compose_context_t *ctx)
{
	messages_page_context_t *base = &ctx->base;
	if (!base->keyboard || lv_obj_has_flag(base->keyboard, LV_OBJ_FLAG_HIDDEN)) { return; }
	lv_obj_t *root = base->root;
	lv_obj_set_style_pad_bottom(root, lv_obj_get_height(base->keyboard) + 8, 0);
	lv_obj_update_layout(lv_obj_get_screen(root));
	lv_area_t input, keyboard;
	lv_obj_get_coords(ctx->body_input, &input);
	lv_obj_get_coords(base->keyboard, &keyboard);
	if (input.y2 >= keyboard.y1) {
		lv_obj_scroll_to_y(root, lv_obj_get_scroll_y(root) + input.y2 - keyboard.y1 + 1,
			LV_ANIM_OFF);
	}
	lv_obj_t *label = lv_textarea_get_label(ctx->body_input);
	lv_point_t cursor;
	lv_obj_update_layout(label);
	lv_label_get_letter_pos(label, lv_textarea_get_cursor_pos(ctx->body_input), &cursor);
	int32_t top = lv_obj_get_scroll_top(ctx->body_input);
	int32_t line_height = lv_font_get_line_height(
		lv_obj_get_style_text_font(ctx->body_input, LV_PART_MAIN));
	int32_t height = lv_obj_get_content_height(ctx->body_input);
	if (cursor.y < top) {
		lv_obj_scroll_to_y(ctx->body_input, cursor.y, LV_ANIM_OFF);
	} else if (cursor.y + line_height > top + height) {
		lv_obj_scroll_to_y(ctx->body_input, cursor.y + line_height - height, LV_ANIM_OFF);
	}
}

static void focus(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	lv_keyboard_set_textarea(ctx->keyboard, lv_event_get_target_obj(event));
	lv_obj_remove_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
	keep_input_visible((compose_context_t *)ctx);
}

static void keyboard_done(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_set_style_pad_bottom(ctx->root, 8, 0);
}

static lv_obj_t *input(messages_page_context_t *ctx, lv_obj_t *root,
	const char *placeholder, const char *value, size_t max, bool single)
{
	lv_obj_t *obj = lv_textarea_create(root);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_style_bg_color(obj, lv_color_hex(0x1b2940), 0);
	lv_obj_set_style_text_color(obj, lv_color_hex(0xe9eff8), 0);
	lv_obj_set_style_border_color(obj, lv_color_hex(0x3b5272), 0);
	lv_textarea_set_one_line(obj, single);
	lv_obj_set_height(obj, single ? 40 : 76);
	lv_textarea_set_max_length(obj, max);
	lv_textarea_set_placeholder_text(obj, placeholder);
	lv_textarea_set_text(obj, value);
	lv_obj_add_event_cb(obj, focus, LV_EVENT_FOCUSED, ctx);
	lv_obj_add_event_cb(obj, focus, LV_EVENT_CLICKED, ctx);
	return obj;
}

static void recipient_open(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->keyboard) {
		lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
		lv_obj_set_style_pad_bottom(ctx->root, 8, 0);
		lv_obj_scroll_to_y(ctx->root, 0, LV_ANIM_OFF);
	}
}

static lv_obj_t *recipient_dropdown(compose_context_t *ctx, lv_obj_t *root)
{
	messages_controller_t *controller = ctx->base.controller;
	ctx->recipient_count = 0;
	size_t used = (size_t)snprintf(ctx->recipient_options, sizeof(ctx->recipient_options), "Select contact");
	uint32_t selected = 0;
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t contact;
	for (size_t i = 0; i < RECIPIENT_CAPACITY; i++) {
		if (contacts_controller_lookup_at(contacts_app_get_controller(), i, &contact)) { break; }
		int added = snprintf(ctx->recipient_options + used, sizeof(ctx->recipient_options) - used,
			"\n%s (%s)", contact.name, contact.phone);
		if (added < 0 || (size_t)added >= sizeof(ctx->recipient_options) - used) { break; }
		used += (size_t)added;
		ctx->recipient_ids[ctx->recipient_count++] = contact.id;
		if (!strcmp(contact.phone, controller->model->editor.phone)) { selected = ctx->recipient_count; }
	}
#else
	(void)used;
#endif
	lv_obj_t *obj = lv_dropdown_create(root);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_height(obj, 40);
	lv_obj_set_style_bg_color(obj, lv_color_hex(0x1b2940), 0);
	lv_obj_set_style_text_color(obj, lv_color_hex(0xe9eff8), 0);
	lv_obj_set_style_border_color(obj, lv_color_hex(0x3b5272), 0);
	lv_dropdown_set_options(obj, ctx->recipient_count ? ctx->recipient_options : "No contacts available");
	lv_dropdown_set_selected(obj, selected);
	lv_obj_t *list = lv_dropdown_get_list(obj);
	lv_obj_set_style_max_height(list, 120, 0);
	lv_obj_set_style_bg_color(list, lv_color_hex(0x1b2940), 0);
	lv_obj_set_style_text_color(list, lv_color_hex(0xe9eff8), 0);
	if (!ctx->recipient_count) {
		lv_obj_add_state(obj, LV_STATE_DISABLED);
		lv_obj_remove_flag(ctx->base.notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(ctx->base.notice, "Add a contact in Contacts first.");
	} else if (!selected && controller->model->editor.phone[0]) {
		lv_obj_remove_flag(ctx->base.notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(ctx->base.notice, "Choose a saved contact for this recipient.");
	}
	return obj;
}


lv_obj_t *messages_compose_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	Page page = lv_page_create("New message", true, messages_page_back, view);
	if (!page.screen || !page.container) { return NULL; }
	messages_page_context_t *base = messages_page_alloc(sizeof(compose_context_t), view);
	if (!base) { return messages_page_finish(view, page, NULL); }
	base->sync = sync_editor;
	messages_page_prepare(base, page, "New message");
	compose_context_t *compose = (compose_context_t *)base;
	lv_obj_t *root = base->root;
	messages_controller_t *controller = base->controller;
	lv_obj_t *header_right = lv_page_header_right(root);
	if (header_right) {
		lv_page_set_title(root, "New message", 40);
		compose->send_button = messages_page_button(base, header_right, "", ACT_SEND, 0x346bd3);
		lv_obj_clean(compose->send_button);
		lv_obj_set_size(compose->send_button, 40, LV_PAGE_HEADER_HEIGHT);
		lv_obj_set_style_bg_opa(compose->send_button, LV_OPA_TRANSP, 0);
		lv_obj_set_style_bg_opa(compose->send_button, LV_OPA_TRANSP, LV_STATE_PRESSED);
		lv_obj_set_style_border_width(compose->send_button, 0, 0);
		lv_obj_set_style_pad_all(compose->send_button, 0, 0);
		lv_obj_t *icon = lv_image_create(compose->send_button);
		lv_image_set_src(icon, &ic_send_28);
		lv_obj_center(icon);
	}
	compose->recipient_select = recipient_dropdown(compose, root);
	compose->body_input = input(base, root, "Write a message...",
		controller->model->editor.body, BEPIO_MESSAGE_BODY_MAX, false);
	lv_obj_add_event_cb(compose->recipient_select, edited, LV_EVENT_VALUE_CHANGED, compose);
	lv_obj_add_event_cb(compose->recipient_select, recipient_open, LV_EVENT_CLICKED, base);
	lv_obj_add_event_cb(compose->recipient_select, recipient_open, LV_EVENT_FOCUSED, base);
	lv_obj_add_event_cb(compose->body_input, edited, LV_EVENT_VALUE_CHANGED, compose);
	if (compose->send_button &&
	    (!lv_dropdown_get_selected(compose->recipient_select) || !controller->model->editor.body[0])) {
		lv_obj_add_state(compose->send_button, LV_STATE_DISABLED);
	}
	base->keyboard = lv_keyboard_create(lv_obj_get_screen(root));
	lv_obj_set_size(base->keyboard, lv_pct(100), 112);
	lv_obj_align(base->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_add_flag(base->keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(base->keyboard, keyboard_done, LV_EVENT_READY, base);
	lv_obj_add_event_cb(base->keyboard, keyboard_done, LV_EVENT_CANCEL, base);
	return messages_page_finish(view, page, base);
}

