#include "settings_home.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "bepio_account.h"
#include "../app.h"
#include <stdio.h>
#include <string.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *id, *state;
	settings_page_t pages[8];
} home_context_t;
static void open_page(lv_event_t *event)
{
	home_context_t *ctx = lv_event_get_user_data(event);
	int32_t index = lv_obj_get_index(lv_event_get_target_obj(event));
	if (index < 0 || index >= (int32_t)(sizeof(ctx->pages) / sizeof(ctx->pages[0]))) { return; }
	settings_page_t page = ctx->pages[index];
	bepio_settings_controller_page(settings_app_get_controller(),
		page);
}
void bepio_settings_home_refresh(settings_page_context_t *base)
{
	home_context_t *ctx = (home_context_t *)base;
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	if (strcmp(lv_label_get_text(ctx->id), account.id)) {
		lv_label_set_text(ctx->id, account.id);
	}
	char state[64];
	snprintf(state, sizeof(state), "%s  /  %s",
		account.online ? "Online" : "Offline", account.confirmed ? "Registered" : "Registration pending");
	if (strcmp(lv_label_get_text(ctx->state), state)) {
		lv_label_set_text(ctx->state, state);
	}
}
lv_obj_t *bepio_settings_home_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Settings", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Settings");
	home_context_t *ctx = (home_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	ctx->base.refresh = bepio_settings_home_refresh;
	ctx->base.refresh_period_ms = 1000;
	lv_obj_t *root = ctx->base.root;
	lv_obj_set_style_pad_all(root, 4, 0);
	lv_obj_set_style_pad_row(root, 2, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	lv_obj_t *card = bepio_ui_button(root, "", 0, 0, lv_obj_get_width(root) - 8, 44,
		open_page, ctx);
	ctx->pages[0] = SETTINGS_PAGE_ID;
	lv_obj_set_style_bg_color(card, lv_color_hex(0xe4eadc), 0);
	ctx->id = bepio_ui_label(card, "", 10, 3, 284);
	ctx->state = bepio_ui_label(card, "", 10, 24, 284);
	for (uint32_t i = 0; i < lv_obj_get_child_count(card); i++) {
		lv_obj_t *label = lv_obj_get_child(card, i);
		lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
	}
	const char *names[] = {"Wi-Fi", "Datetime", "Updates", "Battery", "Storage", "Usage", "About"};
	const settings_page_t pages[] = {SETTINGS_PAGE_WIFI, SETTINGS_PAGE_DATETIME, SETTINGS_PAGE_UPDATES,
		SETTINGS_PAGE_BATTERY, SETTINGS_PAGE_STORAGE, SETTINGS_PAGE_USAGE, SETTINGS_PAGE_ABOUT};
	for (unsigned i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
		lv_obj_t *row = bepio_ui_button(root, "", 0, 0, lv_obj_get_width(root) - 8, 33,
			open_page, ctx);
		ctx->pages[i + 1] = pages[i];
		lv_obj_set_style_bg_color(row, lv_color_hex(0xe4eadc), 0);
		bepio_ui_label(row, names[i], 10, 8, 254);
		bepio_ui_label(row, LV_SYMBOL_RIGHT, lv_obj_get_width(root) - 32, 8, 16);
	}
	bepio_settings_home_refresh(&ctx->base);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
