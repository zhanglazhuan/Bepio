#include "settings_updates.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
/* Reserved example.com endpoint. This page never starts network/flash work. */
static const char *mock_url = "https://updates.example.com/bepio/manifest.json";
typedef struct {
	settings_page_context_t base;
	lv_obj_t *status, *button, *label, *bar;
	unsigned progress;
	bool available;
} updates_context_t;
static void finish_timer(lv_timer_t *timer)
{
	updates_context_t *ctx = lv_timer_get_user_data(timer);
	ctx->base.timer = NULL;
	lv_timer_delete(timer);
}
static void checked(lv_timer_t *timer)
{
	updates_context_t *ctx = lv_timer_get_user_data(timer);
	finish_timer(timer);
	ctx->available = true;
	lv_label_set_text(ctx->status, "Mock update available: v1.1.0");
	lv_label_set_text(ctx->label, "Simulate update"); lv_obj_center(ctx->label);
	lv_obj_remove_state(ctx->button, LV_STATE_DISABLED);
}
static void downloading(lv_timer_t *timer)
{
	updates_context_t *ctx = lv_timer_get_user_data(timer);
	ctx->progress += 20;
	lv_bar_set_value(ctx->bar, ctx->progress, LV_ANIM_OFF);
	lv_label_set_text_fmt(ctx->status, "Simulating update... %u%%", ctx->progress);
	if (ctx->progress >= 100) {
		finish_timer(timer);
		lv_label_set_text(ctx->status, "Demo complete; firmware unchanged");
		lv_label_set_text(ctx->label, "Demo complete"); lv_obj_center(ctx->label);
	}
}
static void run(lv_event_t *event)
{
	updates_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->base.timer) { return; }
	lv_obj_add_state(ctx->button, LV_STATE_DISABLED);
	if (!ctx->available) {
		lv_label_set_text(ctx->status, "Checking mock HTTPS manifest...");
		ctx->base.timer = lv_timer_create(checked, 600, ctx);
	} else {
		ctx->progress = 0;
		lv_label_set_text(ctx->status, "Simulating update... 0%");
		ctx->base.timer = lv_timer_create(downloading, 250, ctx);
	}
	if (!ctx->base.timer) {
		lv_label_set_text(ctx->status, "Cannot start demo; try again");
		lv_obj_remove_state(ctx->button, LV_STATE_DISABLED);
	}
}
lv_obj_t *bepio_settings_updates_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Updates", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Updates");
	updates_context_t *ctx = (updates_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	lv_obj_t *root = ctx->base.root;
	bepio_ui_label(root, "Remote OTA / MOCK", 12, 4, 296);
	bepio_ui_label(root, mock_url, 12, 27, 296);
	ctx->status = bepio_ui_label(root, "Demo only; no download or restart", 12, 76, 296);
	ctx->bar = lv_bar_create(root);
	lv_obj_set_pos(ctx->bar, 12, 105); lv_obj_set_size(ctx->bar, 296, 8);
	lv_bar_set_range(ctx->bar, 0, 100); lv_bar_set_value(ctx->bar, 0, LV_ANIM_OFF);
	ctx->button = bepio_ui_button(root, "Check for updates", 12, 130, 296, 30, run, ctx);
	ctx->label = lv_obj_get_child(ctx->button, 0);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
