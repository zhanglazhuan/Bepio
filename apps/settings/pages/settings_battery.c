#include "settings_battery.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "../app.h"
static void changed(lv_event_t *event)
{
	settings_controller_set(settings_app_get_controller(), "power_save",
		lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED));
}
lv_obj_t *bepio_settings_battery_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Battery", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Battery");
	settings_page_context_t *ctx = bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	lv_obj_t *root = ctx->root;
	bepio_ui_label(root, "Power saving", 12, 16, 220);
	lv_obj_t *toggle = lv_switch_create(root);
	lv_obj_set_pos(toggle, 250, 12); lv_obj_set_size(toggle, 50, 28);
	if (ctx->model->power_save) { lv_obj_add_state(toggle, LV_STATE_CHECKED); }
	lv_obj_add_event_cb(toggle, changed, LV_EVENT_VALUE_CHANGED, NULL);
	bepio_ui_label(root, "Turn off the screen after 30 seconds of inactivity. Touch once to wake it.", 12, 60, 296);
	bepio_ui_label(root, "Does not suspend the modem or audio.\nPreference resets after restart.", 12, 118, 296);
	return bepio_settings_page_finish(view, page, ctx);
}
