#include "view.h"
#include "bepio_log.h"
BEPIO_LOG_TAG(messages_view);
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
#include "controller.h"
#include "lvgl.h"
#include "lv_page.h"
#ifdef CONFIG_BEPIO_APP_CONTACTS
#include "apps/contacts/app.h"
#define RECIPIENT_CAPACITY CONFIG_BEPIO_CONTACTS_CAPACITY
#else
#define RECIPIENT_CAPACITY 1
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef ESP_PLATFORM
#include "voice.h"
#endif

static messages_controller_t *controller;
static lv_obj_t *recipient_select, *body_input, *keyboard, *notice, *send_button;
#ifdef CONFIG_BEPIO_APP_CONTACTS
static uint32_t recipient_ids[RECIPIENT_CAPACITY];
#endif
static size_t recipient_count;
static char recipient_options[40 + RECIPIENT_CAPACITY * (BEPIO_MESSAGE_NAME_MAX + BEPIO_MESSAGE_PHONE_MAX + 5)];
enum { ACT_NEW = 1, ACT_BACK, ACT_REPLY, ACT_DELETE,
	ACT_CONFIRM, ACT_CANCEL, ACT_SAVE, ACT_SEND, ACT_DISCARD, ACT_EDIT, ACT_RETRY };

static lv_obj_t *text(lv_obj_t *parent, const char *value, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, value);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

static void result(int ret)
{
	if (ret && notice && lv_obj_is_valid(notice)) {
		const char *message = ret == -ENOSPC ? "Mailbox full. Delete an old message."
			: ret == -EBUSY ? "Finish the current action first."
			: ret == -ENOTSUP ? "Chinese TTS only."
			: "Check phone and message, then try again.";
		lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(notice, message);
	}
}

static int sync_editor(void)
{
	/* Copy selection through public controllers without rebuilding live widgets. */
	char body[BEPIO_MESSAGE_BODY_MAX + 1];
	snprintf(body, sizeof(body), "%s", lv_textarea_get_text(body_input));
	uint32_t selected = lv_dropdown_get_selected(recipient_select);
	const char *phone = "", *name = "";
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t contact;
	if (selected && selected <= recipient_count) {
		int ret = contacts_controller_lookup(contacts_app_get_controller(), recipient_ids[selected - 1], &contact);
		if (!ret) { phone = contact.phone; name = contact.name; }
		else {
			lv_dropdown_set_selected(recipient_select, 0);
			selected = 0;
			lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
			lv_label_set_text(notice, "Contact unavailable. Choose another contact.");
		}
	}
#endif
	controller->view->available = false;
	int ret = messages_controller_recipient(controller, phone, name);
	if (!ret) { ret = messages_controller_text(controller, body); }
	controller->view->available = true;
	if (send_button) {
		if (selected && !ret) { lv_obj_remove_state(send_button, LV_STATE_DISABLED); }
		else { lv_obj_add_state(send_button, LV_STATE_DISABLED); }
	}
	return ret;
}

static void action(lv_event_t *event)
{
	int code = (int)(uintptr_t)lv_event_get_user_data(event), ret = 0;
	uint32_t id = 0;
	messages_view_t *view = controller->view;
	if (view->page == MESSAGES_PAGE_COMPOSE) {
		ret = sync_editor();
		if (ret) { result(ret); return; }
	}
	switch (code) {
	case ACT_NEW: ret = messages_controller_new(controller, "", ""); break;
	case ACT_BACK: messages_controller_back(controller); break;
	case ACT_REPLY: ret = messages_controller_reply(controller, view->selected_id); break;
	case ACT_DELETE: ret = messages_controller_delete(controller, view->box, view->selected_id); break;
	case ACT_CONFIRM: ret = messages_controller_confirm(controller, &id); break;
	case ACT_CANCEL: ret = messages_controller_cancel(controller); break;
	case ACT_SAVE: ret = messages_controller_save(controller, &id); break;
	case ACT_SEND: ret = messages_controller_send(controller); break;
	case ACT_DISCARD: ret = messages_controller_discard(controller); break;
	case ACT_EDIT: ret = messages_controller_edit(controller, view->selected_id); break;
	case ACT_RETRY: ret = messages_controller_retry(controller, view->selected_id); break;
	}
	result(ret);
}

static lv_obj_t *button(lv_obj_t *parent, const char *value, int code, uint32_t color)
{
	lv_obj_t *obj = lv_button_create(parent);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_height(obj, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
	lv_obj_set_style_radius(obj, 10, 0);
	lv_obj_set_style_shadow_width(obj, 0, 0);
	lv_obj_add_event_cb(obj, action, LV_EVENT_CLICKED, (void *)(uintptr_t)code);
	text(obj, value, 0xffffff);
	return obj;
}

static void edited(lv_event_t *event)
{
	(void)event;
	if (recipient_select && body_input) { result(sync_editor()); }
}

static void focus(lv_event_t *event)
{
	lv_keyboard_set_textarea(keyboard, lv_event_get_target_obj(event));
	lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_done(lv_event_t *event)
{
	(void)event;
	lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *input(lv_obj_t *root, const char *placeholder, const char *value, size_t max, bool single)
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
	lv_obj_add_event_cb(obj, focus, LV_EVENT_FOCUSED, NULL);
	lv_obj_add_event_cb(obj, focus, LV_EVENT_CLICKED, NULL);
	return obj;
}

static void recipient_open(lv_event_t *event)
{
	(void)event;
	if (keyboard) { lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); }
}

static lv_obj_t *recipient_dropdown(lv_obj_t *root)
{
	recipient_count = 0;
	size_t used = (size_t)snprintf(recipient_options, sizeof(recipient_options), "Select contact");
	uint32_t selected = 0;
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t contact;
	for (size_t i = 0; i < RECIPIENT_CAPACITY; i++) {
		if (contacts_controller_lookup_at(contacts_app_get_controller(), i, &contact)) { break; }
		int added = snprintf(recipient_options + used, sizeof(recipient_options) - used,
			"\n%s (%s)", contact.name, contact.phone);
		if (added < 0 || (size_t)added >= sizeof(recipient_options) - used) { break; }
		used += (size_t)added;
		recipient_ids[recipient_count++] = contact.id;
		if (!strcmp(contact.phone, controller->model->editor.phone)) { selected = recipient_count; }
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
	lv_dropdown_set_options(obj, recipient_count ? recipient_options : "No contacts available");
	lv_dropdown_set_selected(obj, selected);
	lv_obj_t *list = lv_dropdown_get_list(obj);
	lv_obj_set_style_max_height(list, 120, 0);
	lv_obj_set_style_bg_color(list, lv_color_hex(0x1b2940), 0);
	lv_obj_set_style_text_color(list, lv_color_hex(0xe9eff8), 0);
	if (!recipient_count) {
		lv_obj_add_state(obj, LV_STATE_DISABLED);
		lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(notice, "Add a contact in Contacts first.");
	} else if (!selected && controller->model->editor.phone[0]) {
		lv_obj_remove_flag(notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(notice, "Choose a saved contact for this recipient.");
	}
	return obj;
}

static void open_message(lv_event_t *event)
{
	bepio_message_t message;
	result(messages_controller_show(controller, controller->view->box,
		(uint32_t)(uintptr_t)lv_event_get_user_data(event), &message));
}

typedef struct {
	bepio_message_box_t box;
	uint32_t id;
} thread_item_t;

static bool has_chinese(const char *value)
{
	for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
		if (*p >= 0xe4 && *p <= 0xe9) { return true; }
	}
	return false;
}

static void play_message(lv_event_t *event)
{
#ifdef ESP_PLATFORM
	uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
	bepio_message_t message;
	if (!bepio_messaging_get(BEPIO_MESSAGE_INBOX, id, false, &message) ||
	    !bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, id, false, &message)) {
		result(bepio_messages_voice_play(message.payload.body));
	}
#else
	(void)event;
#endif
}

static void render_thread(lv_obj_t *root, const bepio_message_t *selected)
{
	thread_item_t items[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY + CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY];
	size_t count = 0;
	for (int box = BEPIO_MESSAGE_INBOX; box <= BEPIO_MESSAGE_OUTBOX; box++) {
		bepio_message_t candidate;
		for (size_t i = 0; !bepio_messaging_get_at((bepio_message_box_t)box, i, &candidate); i++) {
			if (strcmp(candidate.payload.phone, selected->payload.phone)) { continue; }
			size_t at = count++;
			while (at && items[at - 1].id > candidate.id) { items[at] = items[at - 1]; at--; }
			items[at] = (thread_item_t){.box = (bepio_message_box_t)box, .id = candidate.id};
		}
	}
	for (size_t i = 0; i < count; i++) {
		bepio_message_t message;
		if (bepio_messaging_get(items[i].box, items[i].id, false, &message)) { continue; }
		bool outgoing = items[i].box == BEPIO_MESSAGE_OUTBOX;
		lv_obj_t *row = lv_obj_create(root);
		lv_obj_set_width(row, lv_pct(100));
		lv_obj_set_height(row, LV_SIZE_CONTENT);
		lv_obj_set_style_pad_all(row, 0, 0);
		lv_obj_set_style_border_width(row, 0, 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(row, outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
			LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
		lv_obj_t *bubble = lv_obj_create(row);
		lv_obj_set_width(bubble, lv_pct(76));
		lv_obj_set_height(bubble, LV_SIZE_CONTENT);
		lv_obj_set_style_bg_color(bubble, lv_color_hex(outgoing ? 0x265c8e : 0x26354a), 0);
		lv_obj_set_style_border_width(bubble, 0, 0);
		lv_obj_set_style_radius(bubble, 12, 0);
		lv_obj_set_style_pad_all(bubble, 8, 0);
		lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
		if (has_chinese(message.payload.body)) {
			lv_obj_t *play = button(bubble, LV_SYMBOL_PLAY "  Play voice", 0, outgoing ? 0x265c8e : 0x26354a);
			lv_obj_remove_event_cb(play, action);
			lv_obj_add_event_cb(play, play_message, LV_EVENT_CLICKED, (void *)(uintptr_t)message.id);
		} else {
			text(bubble, message.payload.body[0] ? message.payload.body : "Empty draft", 0xe9eff8);
		}
		text(bubble, bepio_messaging_state_label(&message), 0xa8bad2);
	}
}

static void render_ui(const messages_view_t *view)
{
	controller = view->controller;
	if (!controller) { return; }
	if (keyboard) { lv_obj_delete(keyboard); keyboard = NULL; }
	recipient_select = body_input = send_button = NULL;
	lv_obj_t *root = view->root;
	lv_obj_clean(root);
	lv_obj_set_style_bg_color(root, lv_color_hex(0x101827), 0);
	lv_obj_set_style_pad_all(root, 8, 0);
	lv_obj_set_style_pad_row(root, 8, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(root, LV_DIR_VER);
	notice = text(root, "", 0xffb4a9);
	lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);

	char line[128];
	bool listing = view->page == MESSAGES_PAGE_HOME || view->page == MESSAGES_PAGE_BOX;
	lv_obj_t *header_right = bepio_page_header_right(root);
	if (header_right) {
		lv_obj_clean(header_right);
		bepio_page_set_title(root, view->page == MESSAGES_PAGE_COMPOSE ? "New message" : "Messages", listing ? 40 : 0);
		if (listing) {
			lv_obj_t *add = button(header_right, LV_SYMBOL_PLUS, ACT_NEW, 0x346bd3);
			lv_obj_set_size(add, 40, BEPIO_PAGE_HEADER_HEIGHT);
			lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, 0);
			lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, LV_STATE_PRESSED);
			lv_obj_set_style_border_width(add, 0, 0);
			lv_obj_set_style_pad_all(add, 0, 0);
			lv_obj_t *label = lv_obj_get_child(add, 0);
			lv_obj_set_style_text_color(label, lv_obj_get_style_text_color(lv_obj_get_parent(header_right), 0), 0);
			lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
			lv_obj_center(label);
		}
	}
	if (listing) {
		if (!header_right) { button(root, LV_SYMBOL_PLUS " New", ACT_NEW, 0x346bd3); }
		size_t matches = 0;
		bepio_message_t message;
		for (size_t i = 0; !bepio_messaging_get_at(view->box, i, &message); i++) {
			bool already_listed = false;
			bepio_message_t earlier;
			for (size_t j = 0; j < i; j++) {
				if (!bepio_messaging_get_at(view->box, j, &earlier) &&
				    !strcmp(earlier.payload.phone, message.payload.phone)) { already_listed = true; break; }
			}
			if (already_listed) { continue; }
			lv_obj_t *card = button(root, "", 0, 0x1b2940);
			lv_obj_clean(card);
			lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
			lv_obj_remove_event_cb(card, action);
			lv_obj_add_event_cb(card, open_message, LV_EVENT_CLICKED, (void *)(uintptr_t)message.id);
			snprintf(line, sizeof(line), "%s%s", view->box == BEPIO_MESSAGE_INBOX && !message.read ? "* " : "", message.payload.name[0] ? message.payload.name : message.payload.phone);
			text(card, line, 0xe9eff8);
			lv_obj_t *body = text(card, has_chinese(message.payload.body) ? LV_SYMBOL_PLAY " Voice message" :
				message.payload.body[0] ? message.payload.body : "Empty draft", 0xa8bad2);
			lv_label_set_long_mode(body, LV_LABEL_LONG_DOT);
			lv_obj_set_height(body, 18);
			text(card, bepio_messaging_state_label(&message), 0x73d7b6);
			matches++;
		}
		if (!matches) { text(root, "No messages yet", 0xa8bad2); }
	} else if (view->page == MESSAGES_PAGE_DETAIL) {
		bepio_message_t message;
		if (!bepio_page_has_header(root)) { button(root, LV_SYMBOL_LEFT "  Back", ACT_BACK, 0x263c5c); }
		if (bepio_messaging_get(view->box, view->selected_id, false, &message)) { text(root, "Message no longer exists", 0xffb4a9); return; }
		text(root, message.payload.name[0] ? message.payload.name : message.payload.phone, 0x73d7b6);
		text(root, message.payload.phone, 0xa8bad2);
		render_thread(root, &message);
		if (view->box == BEPIO_MESSAGE_INBOX) { button(root, "Reply", ACT_REPLY, 0x346bd3); }
		if (message.state == BEPIO_MESSAGE_DRAFT) { button(root, "Edit draft", ACT_EDIT, 0x346bd3); }
		if (message.state == BEPIO_MESSAGE_FAILED) { button(root, "Retry", ACT_RETRY, 0x346bd3); }
		if (message.state != BEPIO_MESSAGE_SENDING) { button(root, "Delete message", ACT_DELETE, 0x8d3e4a); }
	} else if (view->page == MESSAGES_PAGE_COMPOSE) {
		if (!bepio_page_has_header(root)) { button(root, LV_SYMBOL_LEFT "  Back", ACT_BACK, 0x263c5c); }
		recipient_select = recipient_dropdown(root);
		body_input = input(root, "Write a message...", controller->model->editor.body, BEPIO_MESSAGE_BODY_MAX, false);
		lv_obj_add_event_cb(recipient_select, edited, LV_EVENT_VALUE_CHANGED, NULL);
		lv_obj_add_event_cb(recipient_select, recipient_open, LV_EVENT_CLICKED, NULL);
		lv_obj_add_event_cb(recipient_select, recipient_open, LV_EVENT_FOCUSED, NULL);
		lv_obj_add_event_cb(body_input, edited, LV_EVENT_VALUE_CHANGED, NULL);
		button(root, "Save draft", ACT_SAVE, 0x263c5c);
		send_button = button(root, "Review & queue", ACT_SEND, 0x346bd3);
		if (!lv_dropdown_get_selected(recipient_select)) { lv_obj_add_state(send_button, LV_STATE_DISABLED); }
		text(root, "Offline: queued messages wait here.", 0x93a7c2);
	} else {
		messages_pending_t pending = controller->model->pending;
		text(root, pending == MESSAGES_PENDING_DELETE ? "DELETE MESSAGE?" : pending == MESSAGES_PENDING_LEAVE ? "UNSAVED MESSAGE" : "REVIEW MESSAGE", 0x73d7b6);
		if (pending == MESSAGES_PENDING_LEAVE) {
			button(root, "Save draft", ACT_SAVE, 0x346bd3);
			button(root, "Discard changes", ACT_DISCARD, 0x8d3e4a);
		} else {
			bepio_message_payload_t payload = controller->model->editor;
			bepio_message_t message;
			if (pending == MESSAGES_PENDING_DELETE && !bepio_messaging_get(controller->model->pending_box, controller->model->pending_id, false, &message)) { payload = message.payload; }
			text(root, payload.phone, 0xa8bad2);
			text(root, payload.body, 0xe9eff8);
			button(root, pending == MESSAGES_PENDING_DELETE ? "Confirm delete" : "Confirm queue", ACT_CONFIRM, 0x346bd3);
		}
		button(root, "Cancel", ACT_CANCEL, 0x263c5c);
	}
	keyboard = lv_keyboard_create(lv_obj_get_screen(root));
	lv_obj_set_size(keyboard, lv_pct(100), 112);
	lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, bepio_page_has_header(root) ? -BEPIO_PAGE_BOTTOM_HEIGHT : 0);
	lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_READY, NULL);
	lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_CANCEL, NULL);
}
#endif

void messages_view_init(messages_view_t *view, void *root, void *group, bool editing)
{
	*view = (messages_view_t){.page = editing ? MESSAGES_PAGE_COMPOSE : MESSAGES_PAGE_HOME,
		.available = true, .root = root, .group = group};
}

void messages_view_deinit(messages_view_t *view)
{
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
	if (keyboard) { lv_obj_delete(keyboard); keyboard = NULL; }
	lv_obj_t *header_right = bepio_page_header_right(view->root);
	if (header_right) { lv_obj_clean(header_right); }
	controller = NULL;
#endif
	*view = (messages_view_t){0};
}

void messages_view_render(const messages_view_t *view)
{
	if (!view->available) { return; }
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_PREVIEW)
	if (view->root) { render_ui(view); }
#endif
	bepio_messaging_stats_t stats;
	bepio_messaging_stats(&stats);
	BEPIO_LOGI("Messages: page=%d inbox=%u unread=%u outbox=%u queued=%u [RAM-only]", view->page,
		(unsigned)stats.inbox, (unsigned)stats.unread, (unsigned)stats.outbox, (unsigned)stats.queued);
}
