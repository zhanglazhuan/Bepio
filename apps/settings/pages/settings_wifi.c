/* Settings flow inspired by NomadCast view_wifi; service owns all radio work. */
#include "settings_wifi.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "../app.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *toggle, *status, *networks;
	bepio_wifi_snapshot_t snapshot;
	uint32_t revision;
} wifi_context_t;
static void show_error(wifi_context_t *ctx, int error)
{
	if (error) { lv_label_set_text(ctx->status, error == -EBUSY ? "Wi-Fi is busy; please wait" : "Cannot start Wi-Fi; please retry"); }
}
static void enabled_changed(lv_event_t *event)
{
	wifi_context_t *ctx = lv_event_get_user_data(event);
	int error = bepio_wifi_set_enabled(lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED));
	bepio_settings_wifi_refresh(&ctx->base);
	show_error(ctx, error);
}
static void selected(lv_event_t *event)
{
	wifi_context_t *ctx = lv_event_get_user_data(event);
	size_t at = lv_obj_get_index(lv_event_get_target_obj(event));
	if (at >= ctx->snapshot.count) { return; }
	bepio_wifi_ap_t ap = ctx->snapshot.aps[at];
	if (!ap.supported) { lv_label_set_text(ctx->status, "This Wi-Fi security type is not supported"); return; }
	if (ap.saved || !ap.secured) {
		int error = bepio_wifi_connect(&ap, ap.saved ? NULL : "");
		if (error) { show_error(ctx, error); return; }
	}
	bepio_settings_controller_wifi_select(settings_app_get_controller(), &ap);
}
void bepio_settings_wifi_refresh(settings_page_context_t *base)
{
	wifi_context_t *ctx = (wifi_context_t *)base;
	bepio_wifi_snapshot(&ctx->snapshot);
	bepio_wifi_snapshot_t *snapshot = &ctx->snapshot;
	if (ctx->revision == snapshot->revision) { return; }
	ctx->revision = snapshot->revision;
	if (snapshot->enabled) { lv_obj_add_state(ctx->toggle, LV_STATE_CHECKED); }
	else { lv_obj_remove_state(ctx->toggle, LV_STATE_CHECKED); }
	if (!snapshot->enabled) {
		lv_label_set_text(ctx->status, "");
	} else if (snapshot->scanning) {
		lv_label_set_text(ctx->status, "Scanning...");
	} else if (snapshot->connected) {
		lv_label_set_text_fmt(ctx->status, "%s  %s%s", snapshot->ssid, snapshot->ip,
			snapshot->storage_error ? " (save failed)" : "");
	} else { lv_label_set_text(ctx->status, snapshot->status); }
	lv_obj_set_style_text_align(ctx->status, snapshot->scanning ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_clean(ctx->networks);
	if (!snapshot->enabled) { return; }
	if (snapshot->scanning) { return; }
	for (size_t i = 0; i < snapshot->count; i++) {
		const bepio_wifi_ap_t *ap = &snapshot->aps[i];
		lv_obj_t *row = bepio_ui_button(ctx->networks, "", 0, i * 37, 304, 33, selected, ctx);
		lv_obj_set_style_bg_color(row, lv_color_hex(0xe4eadc), 0);
		lv_obj_t *ssid = bepio_ui_label(row, ap->ssid, 10, 8, 284);
		lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
		if (snapshot->connecting) { lv_obj_add_state(row, LV_STATE_DISABLED); }
	}
}
lv_obj_t *bepio_settings_wifi_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Wi-Fi", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Wi-Fi");
	wifi_context_t *ctx = (wifi_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	ctx->base.refresh = bepio_settings_wifi_refresh;
	ctx->base.refresh_period_ms = 250;
	lv_obj_t *root = ctx->base.root;
	bepio_ui_label(root, "Wi-Fi", 8, 9, 80);
	ctx->toggle = lv_switch_create(root); lv_obj_set_pos(ctx->toggle, 264, 4); lv_obj_set_size(ctx->toggle, 48, 28);
	lv_obj_add_event_cb(ctx->toggle, enabled_changed, LV_EVENT_VALUE_CHANGED, ctx);
	ctx->status = bepio_ui_label(root, "", 8, 38, 304);
	lv_label_set_long_mode(ctx->status, LV_LABEL_LONG_DOT);
	ctx->networks = lv_obj_create(root);
	lv_obj_set_pos(ctx->networks, 8, 62); lv_obj_set_size(ctx->networks, 304, lv_obj_get_height(root) - 62);
	lv_obj_set_style_pad_all(ctx->networks, 0, 0); lv_obj_set_style_border_width(ctx->networks, 0, 0);
	lv_obj_set_style_bg_opa(ctx->networks, LV_OPA_TRANSP, 0); lv_obj_set_scroll_dir(ctx->networks, LV_DIR_VER);
	ctx->revision = UINT32_MAX;
	bepio_settings_wifi_refresh(&ctx->base);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
