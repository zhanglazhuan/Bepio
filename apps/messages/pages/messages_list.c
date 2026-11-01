#include "messages_list.h"
#include <stdlib.h>
#include "messages_page_context.h"
#include "messages_page_common.h"
#include <stdio.h>
#include <string.h>

static void open_message(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	bepio_message_t message;
	int ret = messages_controller_show(ctx->controller, ctx->controller->model->flow.box,
		(uint32_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event)),
		&message);
	if (ctx == ctx->controller->view->page_nav.nav_ctx) {
		messages_page_result(ctx, ret);
	}
}


static void messages_list_page_render(messages_page_context_t *ctx)
{
	lv_obj_t *root = ctx->root;
	messages_controller_t *controller = ctx->controller;
	lv_obj_t *header_right = lv_page_header_right(root);
	if (header_right) {
		lv_obj_t *add = messages_page_button(ctx, header_right, LV_SYMBOL_PLUS, ACT_NEW, 0x346bd3);
		lv_obj_set_size(add, 40, LV_PAGE_HEADER_HEIGHT);
		lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, 0);
		lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, LV_STATE_PRESSED);
		lv_obj_set_style_border_width(add, 0, 0);
		lv_obj_set_style_pad_all(add, 0, 0);
		lv_obj_t *label = lv_obj_get_child(add, 0);
		lv_obj_set_style_text_color(label,
			lv_obj_get_style_text_color(lv_obj_get_parent(header_right), 0), 0);
		lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_center(label);
	} else {
		messages_page_button(ctx, root, LV_SYMBOL_PLUS " New", ACT_NEW, 0x346bd3);
	}
	size_t matches = 0;
	bepio_message_t message;
	char line[128];
	for (size_t i = 0; !bepio_messaging_get_at(controller->model->flow.box, i, &message); i++) {
		bool already_listed = false;
		bepio_message_t earlier;
		for (size_t j = 0; j < i; j++) {
			if (!bepio_messaging_get_at(controller->model->flow.box, j, &earlier) &&
			    !strcmp(earlier.payload.phone, message.payload.phone)) {
				already_listed = true; break;
			}
		}
		if (already_listed) { continue; }
		lv_obj_t *card = messages_page_button(ctx, root, "", 0, 0x1b2940);
		lv_obj_clean(card);
		lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
		lv_obj_remove_event_cb(card, messages_page_action);
		lv_obj_set_user_data(card, (void *)(uintptr_t)message.id);
		lv_obj_add_event_cb(card, open_message, LV_EVENT_CLICKED, ctx);
		snprintf(line, sizeof(line), "%s%s",
			controller->model->flow.box == BEPIO_MESSAGE_INBOX && !message.read ? "* " : "",
			message.payload.name[0] ? message.payload.name : message.payload.phone);
		messages_page_text(card, line, 0xe9eff8);
		lv_obj_t *body = messages_page_text(card,
			messages_page_has_chinese(message.payload.body) ? LV_SYMBOL_PLAY " Voice message" :
			message.payload.body[0] ? message.payload.body : "Empty draft", 0xa8bad2);
		lv_label_set_long_mode(body, LV_LABEL_LONG_DOT);
		lv_obj_set_height(body, 18);
		messages_page_text(card, bepio_messaging_state_label(&message), 0x73d7b6);
		matches++;
	}
	if (!matches) { messages_page_text(root, "No messages yet", 0xa8bad2); }
}

lv_obj_t *messages_list_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	messages_page_context_t *ctx = messages_page_alloc(sizeof(messages_page_context_t), view);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = messages_page_begin(ctx, "Messages");
	if (!screen) { free(ctx); return NULL; }
	messages_list_page_render(ctx);
	return messages_page_finish(ctx);
}

