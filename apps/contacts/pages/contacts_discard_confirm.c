#include "contacts_discard_confirm.h"
#include <stdlib.h>
#include "contacts_page_common.h"

static void bepio_contacts_discard_confirm_page_render(bepio_contacts_page_context_t *ctx)
{
	lv_page_set_title(ctx->root, "Discard Changes", 40);
	if (bepio_contacts_page_wide(ctx)) {
		lv_obj_t *title = bepio_contacts_page_text(ctx->root, "Discard changes?", 12, 8, 296, INK);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
		bepio_contacts_page_text(ctx->root, "Your unsaved changes will be lost.", 12, 44, 296, MUTED);
		ctx->error = bepio_contacts_page_text(ctx->root, "", 12, 86, 296, RED);
		bepio_contacts_page_button(ctx, ctx->root, "Keep editing", 12, 118, 144, 38, GREEN, ACT_BACK);
		bepio_contacts_page_button(ctx, ctx->root, "Discard changes", 164, 118, 144, 38, RED,
			ACT_DISCARD);
		return;
	}
	int shift = lv_page_has_header(ctx->root) ? -36 : 0;
	lv_obj_t *title = bepio_contacts_page_text(ctx->root, "Discard changes?", 16, 42 + shift, 208, INK);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
	bepio_contacts_page_text(ctx->root, "Your unsaved changes will be lost.", 16, 88 + shift, 208, MUTED);
	ctx->error = bepio_contacts_page_text(ctx->root, "", 16, 147 + shift, 208, RED);
	bepio_contacts_page_button(ctx, ctx->root, "Keep editing", 16, 178 + shift, 208, 38, GREEN, ACT_BACK);
	bepio_contacts_page_button(ctx, ctx->root, "Discard changes", 16, 226 + shift, 208, 38, RED,
		ACT_DISCARD);
}

lv_obj_t *bepio_contacts_discard_confirm_build(void *app, void *payload)
{
	(void)payload;
	contacts_view_t *view = app;
	bepio_contacts_page_context_t *ctx = bepio_contacts_page_alloc(sizeof(bepio_contacts_page_context_t), view, view->model);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = bepio_contacts_page_begin(ctx, "Discard Changes");
	if (!screen) { free(ctx); return NULL; }
	ctx->rebuilding = true;
	bepio_contacts_discard_confirm_page_render(ctx);
	ctx->rebuilding = false;
	return bepio_contacts_page_finish(ctx);
}
