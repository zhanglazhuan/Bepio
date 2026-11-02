#include "messages_list.h"
#include "messages_page_context.h"
#include "messages_page_common.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	messages_page_context_t base;
	bepio_messaging_stats_t shown;
	lv_timer_t *refresh;
} list_context_t;

static void stop_refresh(lv_event_t *event)
{
	list_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->refresh) {
		lv_timer_delete(ctx->refresh);
		ctx->refresh = NULL;
	}
}

static void refresh_if_changed(lv_timer_t *timer)
{
	list_context_t *ctx = lv_timer_get_user_data(timer);
	messages_view_t *view = ctx->base.view;
	if (!view->available || view->page_nav.nav_ctx != ctx ||
	    lv_obj_get_screen(ctx->base.root) != lv_screen_active()) return;
	bepio_messaging_stats_t current;
	bepio_messaging_stats(&current);
	if (current.inbox == ctx->shown.inbox && current.outbox == ctx->shown.outbox &&
	    current.unread == ctx->shown.unread && current.queued == ctx->shown.queued) return;
	lv_timer_pause(timer);
	page_navigator_navigate_to(&view->page_nav, view,
		ctx->base.controller->model->flow.page + 1, NULL);
}

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

lv_obj_t *messages_list_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	messages_controller_t *controller = view->controller;
	Page page = lv_page_create("Messages",
		controller->model->flow.page == MESSAGES_PAGE_BOX,
		messages_page_back, view);
	if (!page.screen || !page.container) { return NULL; }
	list_context_t *list = (list_context_t *)messages_page_alloc(sizeof(*list), view);
	messages_page_context_t *ctx = list ? &list->base : NULL;
	if (!ctx) { return messages_page_finish(view, page, NULL); }
	messages_page_prepare(ctx, page, "Messages");
	lv_obj_t *root = ctx->root;
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
		char name[BEPIO_MESSAGE_NAME_MAX + 1];
		messages_model_contact_name(&message, controller->model->flow.box, name, sizeof(name));
		snprintf(line, sizeof(line), "%s%s",
			controller->model->flow.box == BEPIO_MESSAGE_INBOX && !message.read ? "* " : "",
			name);
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
	bepio_messaging_stats(&list->shown);
	list->refresh = lv_timer_create(refresh_if_changed, 1000, list);
	lv_obj_add_event_cb(page.screen, stop_refresh, LV_EVENT_DELETE, list);
	return messages_page_finish(view, page, ctx);
}
