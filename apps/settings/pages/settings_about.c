#include "settings_about.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include "bepio_device_info.h"

lv_obj_t *bepio_settings_about_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("About", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "About");
	settings_page_context_t *ctx = bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	char info[512];
	bepio_device_info(info, sizeof(info));
	bepio_ui_label(ctx->root, info, 12, 8, 296);
	return bepio_settings_page_finish(view, page, ctx);
}
