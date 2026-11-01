/* BSSID/channel selection follows the NomadCast connection flow. */
#include "settings_wifi_connect.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "system/uilv/imgs/icon_24x24/bepio_icons_24.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *password, *keyboard, *notice, *connect_button;
	uint32_t revision;
	bepio_wifi_ap_t ap;
} wifi_connect_context_t;
const bepio_wifi_ap_t *bepio_settings_wifi_connect_ap(const settings_page_context_t *base)
{
	return base ? &((const wifi_connect_context_t *)base)->ap : NULL;
}
static void connect_clicked(lv_event_t *event)
{
	wifi_connect_context_t *ctx = lv_event_get_user_data(event);
	const bepio_wifi_ap_t *ap = &ctx->ap;
	int error = bepio_wifi_connect(ap, ap->secured ? lv_textarea_get_text(ctx->password) : "");
	if (error) {
		lv_label_set_text(ctx->notice, error == -EINVAL ? "8-63 chars / 64 hex digits" :
			error == -EBUSY ? "Connecting..." : "Cannot connect; return to Wi-Fi and retry");
	} else { bepio_settings_wifi_connect_refresh(&ctx->base); }
}
static void focused(lv_event_t *event)
{
	wifi_connect_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_remove_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void keyboard_event(lv_event_t *event)
{
	wifi_connect_context_t *ctx = lv_event_get_user_data(event);
	if (lv_event_get_code(event) == LV_EVENT_READY) { connect_clicked(event); }
	else { lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN); }
}
static void show_password(lv_event_t *event)
{
	wifi_connect_context_t *ctx = lv_event_get_user_data(event);
	lv_obj_t *icon = lv_obj_get_child(lv_event_get_target_obj(event), 0);
	bool hidden = lv_textarea_get_password_mode(ctx->password);
	lv_textarea_set_password_mode(ctx->password, !hidden);
	lv_image_set_src(icon, hidden ? &ic_visibility_24 : &ic_visibility_off_24);
}
void bepio_settings_wifi_connect_refresh(settings_page_context_t *base)
{
	wifi_connect_context_t *ctx = (wifi_connect_context_t *)base;
	bepio_wifi_snapshot_t snapshot; bepio_wifi_snapshot(&snapshot);
	if (ctx->revision == snapshot.revision) { return; }
	ctx->revision = snapshot.revision;
	const bepio_wifi_ap_t *ap = &ctx->ap;
	bool connected = snapshot.connected && !strcmp(ap->ssid, snapshot.ssid);
	if (snapshot.busy) {
		lv_obj_add_state(ctx->connect_button, LV_STATE_DISABLED); lv_obj_add_state(ctx->password, LV_STATE_DISABLED);
		lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_remove_state(ctx->connect_button, LV_STATE_DISABLED);
		if (ap->secured) { lv_obj_remove_state(ctx->password, LV_STATE_DISABLED); }
	}
	if (connected) {
		lv_label_set_text_fmt(ctx->notice, "Connected: %s%s", snapshot.ip, snapshot.storage_error ? " / save failed" : "");
		lv_obj_add_flag(ctx->keyboard, LV_OBJ_FLAG_HIDDEN);
	} else if (snapshot.busy || snapshot.error || !snapshot.enabled) { lv_label_set_text(ctx->notice, snapshot.status); }
	else { lv_label_set_text(ctx->notice, ap->secured ? "" : "Open network; no password needed"); }
}
lv_obj_t *bepio_settings_wifi_connect_build(void *app, void *payload)
{
	const bepio_wifi_ap_t *ap = payload;
	if (!ap || !ap->ssid[0]) { return NULL; }
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Wi-Fi Connect", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Wi-Fi Connect");
	wifi_connect_context_t *ctx = (wifi_connect_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	ctx->ap = *ap;
	ctx->base.refresh = bepio_settings_wifi_connect_refresh;
	ctx->base.refresh_period_ms = 250;
	lv_obj_t *root = ctx->base.root;
	ap = &ctx->ap;
	lv_obj_t *ssid = bepio_ui_label(root, ap->ssid, 8, 0, 304);
	lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
	ctx->password = lv_textarea_create(root);
	lv_obj_t *password = ctx->password;
	lv_obj_set_pos(password, 8, 20); lv_obj_set_size(password, lv_obj_get_width(root) - 16, 31);
	lv_obj_set_style_pad_all(password, 5, 0);
	lv_obj_set_style_pad_right(password, 40, 0);
	lv_textarea_set_one_line(password, true); lv_textarea_set_password_mode(password, true);
	lv_textarea_set_max_length(password, BEPIO_WIFI_PASSWORD_MAX);
	lv_textarea_set_placeholder_text(password, "Password");
	lv_obj_add_event_cb(password, focused, LV_EVENT_FOCUSED, ctx);
	lv_obj_add_event_cb(password, focused, LV_EVENT_CLICKED, ctx);
	lv_obj_t *show = lv_button_create(root);
	lv_obj_set_pos(show, lv_obj_get_width(root) - 43, 20); lv_obj_set_size(show, 32, 31);
	lv_obj_set_style_pad_all(show, 0, 0);
	lv_obj_set_style_bg_opa(show, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(show, 0, 0);
	lv_obj_set_ext_click_area(show, 6);
	lv_obj_t *icon = lv_image_create(show);
	lv_image_set_src(icon, &ic_visibility_off_24);
	lv_obj_set_style_image_recolor(icon, lv_color_hex(0x405d43), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_center(icon);
	lv_obj_add_event_cb(show, show_password, LV_EVENT_CLICKED, ctx);
	ctx->connect_button = bepio_ui_button(root, "Connect", 8, 59, 96, 28, connect_clicked, ctx);
	ctx->notice = bepio_ui_label(root, "", 112, 59, 200);
	lv_label_set_long_mode(ctx->notice, LV_LABEL_LONG_DOT);
	ctx->keyboard = lv_keyboard_create(root);
	lv_obj_t *keyboard = ctx->keyboard;
	lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
	lv_obj_set_pos(keyboard, 0, lv_obj_get_height(root) - 84);
	lv_obj_set_size(keyboard, lv_obj_get_width(root), 84);
	lv_keyboard_set_textarea(keyboard, password);
	lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
	lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_READY, ctx);
	lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_CANCEL, ctx);
	if (!ap->secured) { lv_obj_add_state(password, LV_STATE_DISABLED); lv_obj_add_state(show, LV_STATE_DISABLED); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); }
	ctx->revision = UINT32_MAX;
	bepio_settings_wifi_connect_refresh(&ctx->base);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
