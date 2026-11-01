#include "settings_usage.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "bepio_account.h"
#include <inttypes.h>
typedef struct {
	settings_page_context_t base;
	lv_obj_t *summary;
} usage_context_t;

void bepio_settings_usage_refresh(settings_page_context_t *base)
{
	usage_context_t *ctx = (usage_context_t *)base;
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	lv_label_set_text_fmt(ctx->summary,
		"TX: %" PRIu64 " bytes\nRX: %" PRIu64 " bytes\n"
		"Connected: %" PRIu64 " min %" PRIu64 " sec\nChange cooldown: %lu sec",
		account.tx_bytes, account.rx_bytes,
		account.connected_seconds / 60, account.connected_seconds % 60,
		(unsigned long)account.cooldown_seconds);
}

lv_obj_t *bepio_settings_usage_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Usage", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Usage");
	usage_context_t *ctx = (usage_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	ctx->base.refresh = bepio_settings_usage_refresh;
	ctx->base.refresh_period_ms = 1000;
	lv_obj_t *root = ctx->base.root;
	lv_obj_set_style_pad_hor(root, 12, 0);
	lv_obj_set_style_pad_ver(root, 8, 0);
	lv_obj_set_style_pad_row(root, 12, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	ctx->summary = bepio_ui_label(root, "", 0, 0, 0);
	lv_obj_set_width(ctx->summary, lv_pct(100));
	bepio_settings_usage_refresh(&ctx->base);
	lv_obj_t *notice = bepio_ui_label(root,
		"JSON payload only, excludes carrier overhead.", 0, 0, 0);
	lv_obj_set_width(notice, lv_pct(100));
	return bepio_settings_page_finish(view, page, &ctx->base);
}
