#include "settings_id.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "bepio_account.h"
#include "../app.h"
#include <errno.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *summary, *entry, *notice, *keyboard;
} id_context_t;
void bepio_settings_id_refresh(settings_page_context_t *base)
{
	id_context_t *ctx = (id_context_t *)base;
	bepio_account_snapshot_t a;
	bepio_account_snapshot(&a);
	lv_label_set_text_fmt(ctx->summary, "Account ID: %s\n%s\nConnection: %s\n%s%s",
		a.id, a.confirmed ? "Server confirmed" : "Factory ID (registration pending)",
		a.online ? "Online" : "Offline", a.busy ? "Checking with server..." :
		a.last_result == 0 ? "" : a.last_result == -EEXIST ? "ID already in use" :
		a.last_result == -EAGAIN ? "One change per 24 hours" :
		a.last_result == -ETIMEDOUT ? "Server timeout; ID unchanged" : "Change rejected; ID unchanged",
		a.storage_error ? "\nLocal storage error" : "");
}
static void submit(lv_event_t *event)
{
	id_context_t *ctx = lv_event_get_user_data(event);
	int ret = bepio_account_request(lv_textarea_get_text(ctx->entry));
	lv_label_set_text(ctx->notice, ret == 0 ? "Waiting for server confirmation" :
		ret == -ENETDOWN ? "Offline: backend connection required" :
		ret == -EAGAIN ? "One change per 24 hours" :
		ret == -EBUSY ? "A request is already pending" :
		ret == -EALREADY ? "This is your current ID" : "Use 4-32 lowercase letters, digits or _");
	bepio_settings_id_refresh(&ctx->base);
}
static void focus(lv_event_t *event)
{
	id_context_t *ctx = lv_event_get_user_data(event);
	lv_keyboard_set_textarea(ctx->keyboard, lv_event_get_target_obj(event));
	lv_obj_remove_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void keyboard_done(lv_event_t *event)
{
	id_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *bepio_settings_id_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("ID", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "ID");
	id_context_t *ctx = (id_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	ctx->base.refresh = bepio_settings_id_refresh;
	ctx->base.refresh_period_ms = 1000;
	lv_obj_t *root = ctx->base.root;
	lv_obj_set_style_pad_hor(root, 12, 0);
	lv_obj_set_style_pad_ver(root, 8, 0);
	lv_obj_set_style_pad_row(root, 8, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	ctx->summary = bepio_ui_label(root, "", 0, 0, 0);
	lv_obj_set_width(ctx->summary, lv_pct(100));
	lv_obj_set_style_margin_bottom(ctx->summary, -4, 0);
	bepio_settings_id_refresh(&ctx->base);
	ctx->entry = lv_textarea_create(root);
	lv_obj_set_width(ctx->entry, lv_pct(100));
	lv_textarea_set_one_line(ctx->entry, true);
	lv_textarea_set_max_length(ctx->entry, BEPIO_ACCOUNT_ID_MAX);
	lv_textarea_set_placeholder_text(ctx->entry, "New account ID");
	lv_obj_add_event_cb(ctx->entry, focus, LV_EVENT_FOCUSED, ctx);
	bepio_ui_button(root, "Confirm ID change", 0, 0, 180, 32, submit, ctx);
	ctx->notice = bepio_ui_label(root,
		"4-32: a-z, 0-9, _; once per 24h after approval.", 0, 0, 0);
	lv_obj_set_width(ctx->notice, lv_pct(100));
	ctx->keyboard = lv_keyboard_create(root);
	lv_obj_set_width(ctx->keyboard, lv_pct(100));
	lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(ctx->keyboard, keyboard_done, LV_EVENT_READY, ctx);
	lv_obj_add_event_cb(ctx->keyboard, keyboard_done, LV_EVENT_CANCEL, ctx);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
