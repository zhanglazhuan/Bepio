#include "messages_page_common.h"
#include "messages_page_context.h"
#include <errno.h>
#include <stdlib.h>

messages_page_context_t *messages_page_alloc(size_t size, messages_view_t *view)
{
	if (size < sizeof(messages_page_context_t)) { return NULL; }
	messages_page_context_t *ctx = calloc(1, size);
	if (ctx) { ctx->view = view; ctx->controller = view->controller; }
	return ctx;
}

static void page_back(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	messages_controller_back(ctx->controller);
}

lv_obj_t *messages_page_begin(messages_page_context_t *ctx, const char *title)
{
	Page page = lv_page_create(title,
		ctx->controller->model->flow.page != MESSAGES_PAGE_HOME,
		page_back, ctx);
	if (!page.screen) { return NULL; }
	ctx->root = page.container;
	lv_obj_set_style_bg_color(ctx->root, lv_color_hex(0x101827), 0);
	lv_obj_set_style_pad_all(ctx->root, 8, 0);
	lv_obj_set_style_pad_row(ctx->root, 8, 0);
	lv_obj_set_flex_flow(ctx->root, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scroll_dir(ctx->root, LV_DIR_VER);
	ctx->notice = messages_page_text(ctx->root, "", 0xffb4a9);
	lv_obj_add_flag(ctx->notice, LV_OBJ_FLAG_HIDDEN);
	lv_page_set_title(ctx->root, title,
		ctx->controller->model->flow.page == MESSAGES_PAGE_HOME ||
		ctx->controller->model->flow.page == MESSAGES_PAGE_BOX ? 40 : 0);
	return page.screen;
}

lv_obj_t *messages_page_finish(messages_page_context_t *ctx)
{
	ctx->view->page_nav.nav_ctx = ctx;
	return lv_obj_get_screen(ctx->root);
}

lv_obj_t *messages_page_text(lv_obj_t *parent, const char *value, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, value);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

void messages_page_result(messages_page_context_t *ctx, int ret)
{
	if (ret && ctx == ctx->view->page_nav.nav_ctx && ctx->notice && lv_obj_is_valid(ctx->notice)) {
		const char *message = ret == -ENOSPC ? "Mailbox full. Delete an old message."
			: ret == -EBUSY ? "Finish the current action first."
			: ret == -ENOTSUP ? "Chinese TTS only."
			: "Check phone and message, then try again.";
		lv_obj_remove_flag(ctx->notice, LV_OBJ_FLAG_HIDDEN);
		lv_label_set_text(ctx->notice, message);
	}
}

void messages_page_action(lv_event_t *event)
{
	messages_page_context_t *ctx = lv_event_get_user_data(event);
	messages_controller_t *controller = ctx->controller;
	int code = (int)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event)), ret = 0;
	uint32_t id = 0;
	if (ctx->sync) {
		ret = ctx->sync(ctx);
		if (ret) { messages_page_result(ctx, ret); return; }
	}
	switch (code) {
	case ACT_NEW: ret = messages_controller_new(controller, "", ""); break;
	case ACT_BACK: messages_controller_back(controller); break;
	case ACT_REPLY: ret = messages_controller_reply(controller, controller->model->flow.selected_id); break;
	case ACT_DELETE: ret = messages_controller_delete(controller, controller->model->flow.box, controller->model->flow.selected_id); break;
	case ACT_CONFIRM: ret = messages_controller_confirm(controller, &id); break;
	case ACT_CANCEL: ret = messages_controller_cancel(controller); break;
	case ACT_SAVE: ret = messages_controller_save(controller, &id); break;
	case ACT_SEND: ret = messages_controller_send(controller); break;
	case ACT_DISCARD: ret = messages_controller_discard(controller); break;
	case ACT_EDIT: ret = messages_controller_edit(controller, controller->model->flow.selected_id); break;
	case ACT_RETRY: ret = messages_controller_retry(controller, controller->model->flow.selected_id); break;
	}
	if (ctx == controller->view->page_nav.nav_ctx) { messages_page_result(ctx, ret); }
}

lv_obj_t *messages_page_button(messages_page_context_t *ctx, lv_obj_t *parent,
	const char *value, int code, uint32_t color)
{
	lv_obj_t *obj = lv_button_create(parent);
	lv_obj_set_width(obj, lv_pct(100));
	lv_obj_set_height(obj, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
	lv_obj_set_style_radius(obj, 10, 0);
	lv_obj_set_style_shadow_width(obj, 0, 0);
	lv_obj_set_user_data(obj, (void *)(uintptr_t)code);
	lv_obj_add_event_cb(obj, messages_page_action, LV_EVENT_CLICKED, ctx);
	messages_page_text(obj, value, 0xffffff);
	return obj;
}

bool messages_page_has_chinese(const char *value)
{
	for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
		if (*p >= 0xe4 && *p <= 0xe9) { return true; }
	}
	return false;
}

