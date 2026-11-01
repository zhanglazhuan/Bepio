#include "messages_detail.h"
#include <stdlib.h>
#include "messages_page_context.h"
#include "messages_page_common.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "../voice.h"
#endif

typedef struct {
	bepio_message_box_t box;
	uint32_t id;
} thread_item_t;

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

static void render_thread(messages_page_context_t *ctx, lv_obj_t *root,
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
		if (messages_page_has_chinese(message.payload.body)) {
			lv_obj_t *play = messages_page_button(ctx, bubble, LV_SYMBOL_PLAY "  Play voice", 0, outgoing ? 0x265c8e : 0x26354a);
			lv_obj_remove_event_cb(play, messages_page_action);
			lv_obj_set_user_data(play, (void *)(uintptr_t)message.id);
			lv_obj_add_event_cb(play, play_message, LV_EVENT_CLICKED, ctx);
		} else {
			messages_page_text(bubble, message.payload.body[0] ? message.payload.body : "Empty draft", 0xe9eff8);
		}
		messages_page_text(bubble, bepio_messaging_state_label(&message), 0xa8bad2);
	}
}


static void messages_detail_page_render(messages_page_context_t *ctx)
{
	lv_obj_t *root = ctx->root;
	messages_controller_t *controller = ctx->controller;
	bepio_message_t message;
	if (!lv_page_has_header(root)) {
		messages_page_button(ctx, root, LV_SYMBOL_LEFT "  Back", ACT_BACK, 0x263c5c);
	}
	if (bepio_messaging_get(controller->model->flow.box,
		controller->model->flow.selected_id, false, &message)) {
		messages_page_text(root, "Message no longer exists", 0xffb4a9); return;
	}
	messages_page_text(root, message.payload.name[0] ? message.payload.name :
		message.payload.phone, 0x73d7b6);
	messages_page_text(root, message.payload.phone, 0xa8bad2);
	render_thread(ctx, root, &message);
	if (controller->model->flow.box == BEPIO_MESSAGE_INBOX) {
		messages_page_button(ctx, root, "Reply", ACT_REPLY, 0x346bd3);
	}
	if (message.state == BEPIO_MESSAGE_DRAFT) {
		messages_page_button(ctx, root, "Edit draft", ACT_EDIT, 0x346bd3);
	}
	if (message.state == BEPIO_MESSAGE_FAILED) {
		messages_page_button(ctx, root, "Retry", ACT_RETRY, 0x346bd3);
	}
	if (message.state != BEPIO_MESSAGE_SENDING) {
		messages_page_button(ctx, root, "Delete message", ACT_DELETE, 0x8d3e4a);
	}
}

lv_obj_t *messages_detail_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	messages_page_context_t *ctx = messages_page_alloc(sizeof(messages_page_context_t), view);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = messages_page_begin(ctx, "Messages");
	if (!screen) { free(ctx); return NULL; }
	messages_detail_page_render(ctx);
	return messages_page_finish(ctx);
}

