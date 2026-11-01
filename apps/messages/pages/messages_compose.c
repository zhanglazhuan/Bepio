#include "messages_compose.h"
#include <stdlib.h>
#include "messages_page_context.h"
#include "messages_page_common.h"
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
		if (selected && !ret) { lv_obj_remove_state(ctx->send_button, LV_STATE_DISABLED); }
		else { lv_obj_add_state(ctx->send_button, LV_STATE_DISABLED); }
	}
	return ret;
}

static void edited(lv_event_t *event)
{
	compose_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->recipient_select && ctx->body_input) {
		messages_page_result(&ctx->base, sync_editor(&ctx->base));
	}
}

static void focus(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	lv_keyboard_set_textarea(ctx->keyboard, lv_event_get_target_obj(event));
	lv_obj_remove_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_done(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
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
	if (ctx->keyboard) { lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN); }
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


static messages_page_context_t *messages_compose_page_create(messages_view_t *view)
{
	messages_page_context_t *ctx = messages_page_alloc(sizeof(compose_context_t), view);
	if (ctx) { ctx->sync = sync_editor; }
	return ctx;
}

static void messages_compose_page_render(messages_page_context_t *base)
{
	compose_context_t *compose = (compose_context_t *)base;
	lv_obj_t *root = base->root;
	messages_controller_t *controller = base->controller;
	if (!lv_page_has_header(root)) {
		messages_page_button(base, root, LV_SYMBOL_LEFT "  Back", ACT_BACK, 0x263c5c);
	}
	compose->recipient_select = recipient_dropdown(compose, root);
	compose->body_input = input(base, root, "Write a message...",
		controller->model->editor.body, BEPIO_MESSAGE_BODY_MAX, false);
	lv_obj_add_event_cb(compose->recipient_select, edited, LV_EVENT_VALUE_CHANGED, compose);
	lv_obj_add_event_cb(compose->recipient_select, recipient_open, LV_EVENT_CLICKED, base);
	lv_obj_add_event_cb(compose->recipient_select, recipient_open, LV_EVENT_FOCUSED, base);
	lv_obj_add_event_cb(compose->body_input, edited, LV_EVENT_VALUE_CHANGED, compose);
	messages_page_button(base, root, "Save draft", ACT_SAVE, 0x263c5c);
	compose->send_button = messages_page_button(base, root, "Review & queue", ACT_SEND, 0x346bd3);
	if (!lv_dropdown_get_selected(compose->recipient_select)) {
		lv_obj_add_state(compose->send_button, LV_STATE_DISABLED);
	}
	messages_page_text(root, "Offline: queued messages wait here.", 0x93a7c2);
	base->keyboard = lv_keyboard_create(lv_obj_get_screen(root));
	lv_obj_set_size(base->keyboard, lv_pct(100), 112);
	lv_obj_align(base->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_add_flag(base->keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(base->keyboard, keyboard_done, LV_EVENT_READY, base);
	lv_obj_add_event_cb(base->keyboard, keyboard_done, LV_EVENT_CANCEL, base);
}

lv_obj_t *messages_compose_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	messages_page_context_t *ctx = messages_compose_page_create(view);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = messages_page_begin(ctx, "New message");
	if (!screen) { free(ctx); return NULL; }
	messages_compose_page_render(ctx);
	return messages_page_finish(ctx);
}

