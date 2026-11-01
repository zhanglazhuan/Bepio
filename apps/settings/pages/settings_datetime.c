#include "settings_datetime.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "../app.h"
#include <time.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *h24, *h12;
} datetime_context_t;
static void timezone_changed(lv_event_t *event)
{
	bepio_settings_controller_timezone(settings_app_get_controller(),
		lv_dropdown_get_selected(lv_event_get_target_obj(event)));
}
static void format_changed(lv_event_t *event)
{
	datetime_context_t *ctx = lv_event_get_user_data(event);
	settings_controller_set(settings_app_get_controller(), "hour_24",
		lv_event_get_target_obj(event) == ctx->h24);
	lv_obj_set_style_bg_color(ctx->base.model->hour_24 ? ctx->h12 : ctx->h24,
		lv_color_hex(0x738295), 0);
	lv_obj_set_style_bg_color(ctx->base.model->hour_24 ? ctx->h24 : ctx->h12,
		lv_color_hex(0x405d43), 0);
}
lv_obj_t *bepio_settings_datetime_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Datetime", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Datetime");
	datetime_context_t *ctx = (datetime_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	lv_obj_t *root = ctx->base.root;
	bepio_ui_label(root, "Time zone", 8, 5, 304);
	lv_obj_t *zone = lv_dropdown_create(root);
	lv_obj_set_pos(zone, 8, 28); lv_obj_set_size(zone, 304, 32);
	lv_dropdown_clear_options(zone);
	for (size_t i = 0; i < bepio_settings_timezone_count; i++) {
		lv_dropdown_add_option(zone, bepio_settings_timezones[i].name, LV_DROPDOWN_POS_LAST);
	}
	lv_dropdown_set_selected(zone, model->timezone);
	lv_obj_add_event_cb(zone, timezone_changed, LV_EVENT_VALUE_CHANGED, NULL);
	bepio_ui_label(root, "Time format", 8, 74, 304);
	lv_obj_t *h24 = bepio_ui_button(root, "24h", 8, 98, 148, 32, format_changed, ctx);
	lv_obj_t *h12 = bepio_ui_button(root, "12h", 164, 98, 148, 32, format_changed, ctx);
	ctx->h24 = h24;
	ctx->h12 = h12;
	lv_obj_set_style_bg_color(model->hour_24 ? h12 : h24, lv_color_hex(0x738295), 0);
	bepio_ui_label(root, time(NULL) < 1704067200 ? "Waiting for clock synchronization" : "Applied to the status bar clock", 8, 140, 304);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
