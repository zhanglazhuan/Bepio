#include "view.h"
#include "pages/contacts_page_common.h"
#include "pages/contacts_list.h"
#include "pages/contacts_detail.h"
#include "pages/contacts_edit.h"
#include "pages/contacts_delete_confirm.h"
#include "pages/contacts_discard_confirm.h"

bepio_contacts_page_context_t bepio_contacts_ui;

static void gui_render(contacts_view_t *view, const contacts_model_t *model)
{
	bepio_contacts_ui.rebuilding = true;
	bepio_contacts_ui.view = view; bepio_contacts_ui.model = model; bepio_contacts_ui.root = view->root;
	bepio_contacts_ui.list = bepio_contacts_ui.keyboard = bepio_contacts_ui.name = bepio_contacts_ui.phone = bepio_contacts_ui.error = NULL;
	lv_obj_t *header_right = bepio_page_header_right(bepio_contacts_ui.root);
	if (header_right) { lv_obj_clean(header_right); }
	view->search_editing = false;
	lv_obj_clean(bepio_contacts_ui.root);
	lv_obj_set_style_pad_all(bepio_contacts_ui.root, 0, 0);
	lv_obj_set_style_radius(bepio_contacts_ui.root, 0, 0);
	lv_obj_set_style_bg_color(bepio_contacts_ui.root, lv_color_hex(BG), 0);
	lv_obj_add_flag(bepio_contacts_ui.root, LV_OBJ_FLAG_SCROLLABLE);
	if (bepio_contacts_page_wide()) { lv_obj_remove_flag(bepio_contacts_ui.root, LV_OBJ_FLAG_SCROLLABLE); }
	lv_obj_set_scroll_dir(bepio_contacts_ui.root, LV_DIR_VER);
	lv_obj_scroll_to_y(bepio_contacts_ui.root, 0, LV_ANIM_OFF);
	switch (view->page) {
	case CONTACTS_PAGE_LIST: bepio_contacts_list_page_render(); break;
	case CONTACTS_PAGE_DETAIL: bepio_contacts_detail_page_render(); break;
	case CONTACTS_PAGE_EDIT: bepio_contacts_edit_page_render(); break;
	case CONTACTS_PAGE_DELETE_CONFIRM: bepio_contacts_delete_confirm_page_render(); break;
	case CONTACTS_PAGE_DISCARD_CONFIRM: bepio_contacts_discard_confirm_page_render(); break;
	}
	bepio_contacts_ui.rebuilding = false;
}

static void gui_reset(void)
{
	lv_obj_t *header_right = bepio_page_header_right(bepio_contacts_ui.root);
	if (header_right) { lv_obj_clean(header_right); }
	memset(&bepio_contacts_ui, 0, sizeof(bepio_contacts_ui));
}

void contacts_view_init(contacts_view_t *view, void *root, void *group)
{
	*view = (contacts_view_t) {
		.page = CONTACTS_PAGE_LIST, .available = true, .root = root, .group = group,
	};
}

void contacts_view_deinit(contacts_view_t *view)
{
	gui_reset();
	*view = (contacts_view_t) {0};
}

void contacts_view_render(contacts_view_t *view, const contacts_model_t *model)
{
	if (!view->available) {
		return;
	}
	if (view->root) { gui_render(view, model); }
}
