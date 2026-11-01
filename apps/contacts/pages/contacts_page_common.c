#include "contacts_list.h"
#include "contacts_detail.h"
#include "contacts_page_common.h"
#include "contacts_edit.h"
#include <errno.h>
#include <stdlib.h>

bepio_contacts_page_context_t *bepio_contacts_page_alloc(size_t size,
	contacts_view_t *view, contacts_model_t *model)
{
	if (size < sizeof(bepio_contacts_page_context_t)) { return NULL; }
	bepio_contacts_page_context_t *ctx = calloc(1, size);
	if (ctx) { ctx->view = view; ctx->model = model; }
	return ctx;
}

static void page_back(lv_event_t *event)
{
	bepio_contacts_page_context_t *ctx = lv_event_get_user_data(event);
	if (ctx->model->flow.page == CONTACTS_PAGE_EDIT) { bepio_contacts_edit_request_back(ctx); }
	else { contacts_controller_back(contacts_app_get_controller()); }
}

lv_obj_t *bepio_contacts_page_begin(bepio_contacts_page_context_t *ctx,
	const char *title)
{
	Page page = lv_page_create(title, ctx->model->flow.page != CONTACTS_PAGE_LIST,
		page_back, ctx);
	if (!page.screen) { return NULL; }
	ctx->root = page.container;
	lv_obj_set_layout(ctx->root, LV_LAYOUT_NONE);
	lv_obj_set_style_pad_all(ctx->root, 0, 0);
	lv_obj_set_style_radius(ctx->root, 0, 0);
	lv_obj_set_style_bg_color(ctx->root, lv_color_hex(BG), 0);
	lv_obj_set_scroll_dir(ctx->root, LV_DIR_VER);
	return page.screen;
}

lv_obj_t *bepio_contacts_page_finish(bepio_contacts_page_context_t *ctx)
{
	ctx->view->page_nav.nav_ctx = ctx;
	return lv_obj_get_screen(ctx->root);
}

bool bepio_contacts_page_wide(const bepio_contacts_page_context_t *ctx)
{
	return ctx->root && lv_obj_get_width(ctx->root) > 280;
}

lv_obj_t * bepio_contacts_page_text(lv_obj_t *parent, const char *value, int x, int y, int width, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, value);
	lv_obj_set_pos(obj, x, y);
	if (width) { lv_obj_set_width(obj, width); }
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

lv_obj_t * bepio_contacts_page_button(bepio_contacts_page_context_t *ctx,
	lv_obj_t *parent, const char *value, int x, int y,
		       int width, int height, uint32_t color, action_t id)
{
	lv_obj_t *obj = lv_button_create(parent);
	lv_obj_set_pos(obj, x, y);
	lv_obj_set_size(obj, width, height);
	lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
	lv_obj_set_style_radius(obj, 10, 0);
	lv_obj_set_style_shadow_width(obj, 0, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_set_user_data(obj, (void *)(uintptr_t)id);
	lv_obj_add_event_cb(obj, bepio_contacts_page_action, LV_EVENT_CLICKED, ctx);
	lv_obj_center(bepio_contacts_page_text(obj, value, 0, 0, 0, 0xffffff));
	return obj;
}

static const char *error_text(int error)
{
	switch (error) {
	case -EIO: return "Flash save failed. Try again.";
	case -ENOMEM: return "Not enough memory to save.";
	case -EEXIST: return "Number already exists";
	case -ENOSPC: return "Address book is full";
	case -ENAMETOOLONG: return "Text is too long";
	case -ENOENT: return "Contact no longer exists";
	default: return "Check name and number";
	}
}

void bepio_contacts_page_report(bepio_contacts_page_context_t *ctx, int error)
{
	if (error && ctx == ctx->view->page_nav.nav_ctx && ctx->error && lv_obj_is_valid(ctx->error)) {
		lv_label_set_text(ctx->error, error_text(error));
	}
}

void bepio_contacts_page_action(lv_event_t *event)
{
	bepio_contacts_page_context_t *ctx = lv_event_get_user_data(event);
	if (!ctx->view || ctx->rebuilding) { return; }
	action_t id = (action_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event));
	contacts_controller_t *c = contacts_app_get_controller();
	int ret = 0;
	switch (id) {
	case ACT_FAVORITE: ret = bepio_contacts_controller_toggle_favorite(c, ctx->model->flow.selected_id); break;
	case ACT_MESSAGE: {
		contacts_entry_t entry;
		ret = contacts_controller_lookup(c, ctx->model->flow.selected_id, &entry);
		if (!ret) {
			ret = c->compose_message ? c->compose_message(entry.phone, entry.name) : -ENOSYS;
		}
		return;
	}
	case ACT_NEW: ret = contacts_controller_edit(c, 0); break;
	case ACT_EDIT: ret = contacts_controller_edit(c, ctx->model->flow.selected_id); break;
	case ACT_SAVE:
		ret = bepio_contacts_edit_page_sync(ctx);
		if (!ret) { ret = contacts_controller_save(c); }
		break;
	case ACT_BACK:
		if (ctx->model->flow.page == CONTACTS_PAGE_EDIT) { bepio_contacts_edit_request_back(ctx); }
		else { contacts_controller_back(c); }
		break;
	case ACT_DELETE: ret = bepio_contacts_detail_show_delete_sheet(ctx); break;
	case ACT_NEXT: bepio_contacts_list_page_advance(ctx, 1); break;
	case ACT_PREVIOUS: bepio_contacts_list_page_advance(ctx, -1); break;
	}
	bepio_contacts_page_report(ctx, ret);
}

char bepio_contacts_page_initial(const char *name)
{
	unsigned char c = (unsigned char)name[0];
	return c >= 0x20 && c < 0x7f ? (c >= 'a' && c <= 'z' ? c - 32 : c) : '#';
}
