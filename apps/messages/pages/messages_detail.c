#include "messages_detail.h"
#include "messages_page_context.h"
#include "messages_page_common.h"
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "../voice.h"
#endif

typedef struct {
	bepio_message_box_t box;
	uint32_t id;
} thread_item_t;

typedef struct {
	messages_page_context_t base;
	lv_obj_t *delete_button;
} detail_context_t;

static void select_message(lv_event_t *event)
{
	detail_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->delete_button) lv_obj_add_flag(ctx->delete_button, LV_OBJ_FLAG_HIDDEN);
	lv_obj_t *bubble = lv_event_get_target_obj(event);
	lv_obj_t *row = lv_obj_get_parent(bubble);
	bepio_message_box_t box = (bepio_message_box_t)((uintptr_t)
		lv_obj_get_user_data(bubble) - 1);
	ctx->delete_button = lv_obj_get_child(row, box == BEPIO_MESSAGE_OUTBOX ? 0 : 1);
	lv_obj_remove_flag(ctx->delete_button, LV_OBJ_FLAG_HIDDEN);
}

static void delete_message(lv_event_t *event)
{
	detail_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_t *row = lv_obj_get_parent(lv_event_get_target_obj(event));
	lv_obj_t *bubble = lv_obj_get_child(row,
		lv_obj_get_index(lv_event_get_target_obj(event)) == 0 ? 1 : 0);
	bepio_message_box_t box = (bepio_message_box_t)((uintptr_t)
		lv_obj_get_user_data(bubble) - 1);
	uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
	int ret = messages_controller_delete_now(ctx->base.controller, box, id);
	if (ctx == ctx->base.view->page_nav.nav_ctx) messages_page_result(&ctx->base, ret);
}

static void play_message(lv_event_t *event)
{
#ifdef ESP_PLATFORM
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event));
	bepio_message_t message;
	if (!bepio_messaging_get(BEPIO_MESSAGE_INBOX, id, false, &message) ||
	    !bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, id, false, &message)) {
		messages_page_result(ctx, bepio_messages_voice_play(message.payload.body));
	}
#else
	(void)event;
#endif
}

static void render_thread(detail_context_t *ctx, lv_obj_t *root,
	const bepio_message_t *selected)
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
		lv_obj_set_user_data(row, (void *)(uintptr_t)items[i].id);
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
		lv_obj_set_user_data(bubble, (void *)(uintptr_t)(items[i].box + 1));
		lv_obj_add_flag(bubble, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(bubble, select_message, LV_EVENT_CLICKED, ctx);
		if (messages_page_has_chinese(message.payload.body)) {
			lv_obj_t *play = messages_page_button(&ctx->base, bubble, LV_SYMBOL_PLAY "  Play voice", 0, outgoing ? 0x265c8e : 0x26354a);
			lv_obj_remove_event_cb(play, messages_page_action);
			lv_obj_set_user_data(play, (void *)(uintptr_t)message.id);
			lv_obj_add_event_cb(play, play_message, LV_EVENT_CLICKED, ctx);
		} else {
			messages_page_text(bubble, message.payload.body[0] ? message.payload.body : "Empty draft", 0xe9eff8);
		}
		lv_obj_t *remove = lv_button_create(row);
		lv_obj_set_size(remove, 34, 34);
		lv_obj_set_style_bg_color(remove, lv_color_hex(0x8d3e4a), 0);
		lv_obj_set_style_radius(remove, 8, 0);
		lv_obj_set_style_shadow_width(remove, 0, 0);
		lv_obj_add_flag(remove, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_event_cb(remove, delete_message, LV_EVENT_CLICKED, ctx);
		lv_obj_t *icon = lv_label_create(remove);
		lv_label_set_text(icon, LV_SYMBOL_TRASH);
		lv_obj_set_style_text_color(icon, lv_color_hex(0xffffff), 0);
		lv_obj_center(icon);
		if (outgoing) lv_obj_move_to_index(remove, 0);
	}
}


lv_obj_t *messages_detail_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	Page page = lv_page_create("Messages", true, messages_page_back, view);
	if (!page.screen || !page.container) { return NULL; }
	detail_context_t *detail = (detail_context_t *)messages_page_alloc(sizeof(*detail), view);
	messages_page_context_t *ctx = detail ? &detail->base : NULL;
	if (!ctx) { return messages_page_finish(view, page, NULL); }
	messages_page_prepare(ctx, page, "Messages");
	lv_obj_t *root = ctx->root;
	messages_controller_t *controller = ctx->controller;
	bepio_message_t message;
	if (bepio_messaging_get(controller->model->flow.box,
		controller->model->flow.selected_id, false, &message)) {
		messages_page_text(root, "Message no longer exists", 0xffb4a9);
		return messages_page_finish(view, page, ctx);
	}
	char contact[BEPIO_MESSAGE_NAME_MAX + BEPIO_MESSAGE_PHONE_MAX + 3];
	char name[BEPIO_MESSAGE_NAME_MAX + 1];
	messages_model_contact_name(&message, controller->model->flow.box, name, sizeof(name));
	if (strcmp(name, message.payload.phone)) {
		snprintf(contact, sizeof(contact), "%s(%s)", name, message.payload.phone);
	} else {
		snprintf(contact, sizeof(contact), "%s", message.payload.phone);
	}
	lv_obj_t *contact_label = messages_page_text(root, contact, 0x73d7b6);
	lv_label_set_long_mode(contact_label, LV_LABEL_LONG_DOT);
	lv_obj_set_height(contact_label, lv_font_get_line_height(lv_obj_get_style_text_font(contact_label, 0)));
	render_thread(detail, root, &message);
	if (controller->model->flow.box == BEPIO_MESSAGE_INBOX) {
		lv_obj_t *header_right = lv_page_header_right(root);
		if (header_right) {
			lv_page_set_title(root, "Messages", 56);
			lv_obj_t *reply = messages_page_button(ctx, header_right, "Reply", ACT_REPLY, 0x346bd3);
			lv_obj_set_size(reply, 56, LV_PAGE_HEADER_HEIGHT);
			lv_obj_set_style_bg_opa(reply, LV_OPA_TRANSP, 0);
			lv_obj_set_style_bg_opa(reply, LV_OPA_TRANSP, LV_STATE_PRESSED);
			lv_obj_set_style_border_width(reply, 0, 0);
			lv_obj_set_style_pad_all(reply, 0, 0);
			lv_obj_t *label = lv_obj_get_child(reply, 0);
			lv_obj_set_style_text_color(label, lv_color_black(), 0);
			lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
			lv_obj_center(label);
		}
	}
	if (message.state == BEPIO_MESSAGE_DRAFT) {
		messages_page_button(ctx, root, "Edit draft", ACT_EDIT, 0x346bd3);
	}
	if (message.state == BEPIO_MESSAGE_FAILED) {
		messages_page_button(ctx, root, "Retry", ACT_RETRY, 0x346bd3);
	}
	return messages_page_finish(view, page, ctx);
}

