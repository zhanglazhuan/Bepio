#include "contacts_list.h"
#include "contacts_page_common.h"

bool bepio_contacts_page_wide(void) { return bepio_contacts_ui.root && lv_obj_get_width(bepio_contacts_ui.root) > 280; }

lv_obj_t * bepio_contacts_page_text(lv_obj_t *parent, const char *value, int x, int y, int width, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, value);
	lv_obj_set_pos(obj, x, y);
	if (width) { lv_obj_set_width(obj, width); }
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

lv_obj_t * bepio_contacts_page_button(lv_obj_t *parent, const char *value, int x, int y,
		       int width, int height, uint32_t color, action_t id)
{
	lv_obj_t *obj = lv_button_create(parent);
	lv_obj_set_pos(obj, x, y);
	lv_obj_set_size(obj, width, height);
	lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
	lv_obj_set_style_radius(obj, 10, 0);
	lv_obj_set_style_shadow_width(obj, 0, 0);
	lv_obj_set_style_border_width(obj, 0, 0);
	lv_obj_add_event_cb(obj, bepio_contacts_page_action, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
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

void bepio_contacts_page_report(int error)
{
	if (error && bepio_contacts_ui.error) { lv_label_set_text(bepio_contacts_ui.error, error_text(error)); }
}

int bepio_contacts_page_sync_draft(void)
{
	if (!bepio_contacts_ui.name || !bepio_contacts_ui.phone) { return -EINVAL; }
	return contacts_controller_draft(contacts_app_get_controller(),
		lv_textarea_get_text(bepio_contacts_ui.name), lv_textarea_get_text(bepio_contacts_ui.phone));
}

void bepio_contacts_page_action(lv_event_t *event)
{
	if (!bepio_contacts_ui.view || bepio_contacts_ui.rebuilding) { return; }
	action_t id = (action_t)(uintptr_t)lv_event_get_user_data(event);
	contacts_controller_t *c = contacts_app_get_controller();
	int ret = 0;
	switch (id) {
	case ACT_FAVORITE: ret = bepio_contacts_controller_toggle_favorite(c, bepio_contacts_ui.view->selected_id); break;
	case ACT_MESSAGE: {
		contacts_entry_t entry;
		ret = contacts_controller_lookup(c, bepio_contacts_ui.view->selected_id, &entry);
		if (!ret) { ret = bepio_page_compose_message(entry.phone, entry.name); }
		return;
	}
	case ACT_NEW: ret = contacts_controller_edit(c, 0); break;
	case ACT_EDIT: ret = contacts_controller_edit(c, bepio_contacts_ui.view->selected_id); break;
	case ACT_SAVE:
		ret = bepio_contacts_page_sync_draft();
		if (!ret) { ret = contacts_controller_save(c); }
		break;
	case ACT_BACK: contacts_controller_back(c); break;
	case ACT_DELETE: ret = contacts_controller_request_delete(c); break;
	case ACT_DELETE_YES: ret = contacts_controller_confirm_delete(c); break;
	case ACT_DISCARD: ret = contacts_controller_discard(c); break;
	case ACT_NEXT: bepio_contacts_ui.offset += PAGE_SIZE; bepio_contacts_list_page_refresh(); break;
	case ACT_PREVIOUS: bepio_contacts_ui.offset = bepio_contacts_ui.offset >= PAGE_SIZE ? bepio_contacts_ui.offset - PAGE_SIZE : 0; bepio_contacts_list_page_refresh(); break;
	}
	bepio_contacts_page_report(ret);
}

char bepio_contacts_page_initial(const char *name)
{
	unsigned char c = (unsigned char)name[0];
	return c >= 0x20 && c < 0x7f ? (c >= 'a' && c <= 'z' ? c - 32 : c) : '#';
}
