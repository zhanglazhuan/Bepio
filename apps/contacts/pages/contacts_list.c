#include "contacts_list.h"
#include "contacts_page_common.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"

static void open_contact(lv_event_t *event)
{
	if (!bepio_contacts_ui.view) { return; }
	uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
	contacts_entry_t entry;
	bepio_contacts_page_report(contacts_controller_show(contacts_app_get_controller(), id, &entry));
}

static int name_compare(const char *a, const char *b)
{
	while (*a && *b) {
		unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
		if (x >= 'A' && x <= 'Z') { x += 32; }
		if (y >= 'A' && y <= 'Z') { y += 32; }
		if (x != y) { return (int)x - y; }
	}
	return (unsigned char)*a - (unsigned char)*b;
}

void bepio_contacts_list_page_refresh(void)
{
	lv_obj_clean(bepio_contacts_ui.list);
	size_t indexes[CONFIG_BEPIO_CONTACTS_CAPACITY], count = 0;
	for (size_t i = 0; i < bepio_contacts_ui.model->count; i++) {
		{
			size_t at = count;
			while (at && name_compare(bepio_contacts_ui.model->entries[indexes[at - 1]].name, bepio_contacts_ui.model->entries[i].name) > 0) {
				indexes[at] = indexes[at - 1]; at--;
			}
			indexes[at] = i; count++;
		}
	}
	if (!count) {
		lv_obj_t *empty = bepio_contacts_page_text(bepio_contacts_ui.list, "No contact yet, click + goto add",
			8, 30, lv_obj_get_width(bepio_contacts_ui.root) - 16, MUTED);
		lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
		return;
	}
	if (bepio_contacts_ui.offset >= count) { bepio_contacts_ui.offset = 0; }
	int y = 0;
	char previous = 0;
	size_t end = bepio_contacts_ui.offset + PAGE_SIZE < count ? bepio_contacts_ui.offset + PAGE_SIZE : count;
	for (size_t i = bepio_contacts_ui.offset; i < end; i++) {
		const contacts_entry_t *entry = &bepio_contacts_ui.model->entries[indexes[i]];
		char letter = bepio_contacts_page_initial(entry->name), glyph[] = {letter, 0};
		if (!bepio_contacts_page_wide() && letter != previous) { bepio_contacts_page_text(bepio_contacts_ui.list, glyph, 15, y + 3, 0, GREEN); y += 24; previous = letter; }
		lv_obj_t *row = lv_button_create(bepio_contacts_ui.list);
		lv_obj_set_pos(row, bepio_contacts_page_wide() ? 8 : 12, y);
		lv_obj_set_size(row, lv_obj_get_width(bepio_contacts_ui.root) - (bepio_contacts_page_wide() ? 16 : 24), bepio_contacts_page_wide() ? 44 : 58);
		lv_obj_set_style_bg_color(row, lv_color_white(), 0);
		lv_obj_set_style_radius(row, 12, 0);
		lv_obj_set_style_shadow_width(row, 0, 0);
		lv_obj_set_style_pad_all(row, 0, 0);
		lv_obj_add_event_cb(row, open_contact, LV_EVENT_CLICKED, (void *)(uintptr_t)entry->id);
		lv_obj_t *avatar = lv_obj_create(row);
		lv_obj_set_pos(avatar, 10, bepio_contacts_page_wide() ? 5 : 12); lv_obj_set_size(avatar, 34, 34);
		lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
		lv_obj_set_style_border_width(avatar, 0, 0);
		lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_center(bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN));
		lv_obj_t *name = bepio_contacts_page_text(row, entry->name, 54, bepio_contacts_page_wide() ? 13 : 8, bepio_contacts_page_wide() ? 116 : (entry->favorite ? 126 : 156), INK);
		lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
		lv_obj_set_height(name, lv_font_get_line_height(lv_obj_get_style_text_font(name, 0)));
		lv_obj_t *phone = bepio_contacts_page_text(row, entry->phone, bepio_contacts_page_wide() ? 174 : 54, bepio_contacts_page_wide() ? 13 : 32, bepio_contacts_page_wide() ? (entry->favorite ? 88 : 124) : (entry->favorite ? 128 : 156), MUTED);
		lv_label_set_long_mode(phone, LV_LABEL_LONG_DOT);
		lv_obj_set_height(phone, lv_font_get_line_height(lv_obj_get_style_text_font(phone, 0)));
		if (entry->favorite) {
			lv_obj_t *icon = lv_image_create(row);
			lv_image_set_src(icon, &ic_bookmark_star_28);
			lv_obj_set_style_image_recolor(icon, lv_color_hex(0xc48a20), 0);
			lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
			lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -8, 0);
		}
		y += bepio_contacts_page_wide() ? 48 : 64;
	}
	if (bepio_contacts_ui.offset) { bepio_contacts_page_button(bepio_contacts_ui.list, "Previous", 12, y + 8, 100, 34, MUTED, ACT_PREVIOUS); }
	if (end < count) { bepio_contacts_page_button(bepio_contacts_ui.list, "More", 124, y + 8, 104, 34, GREEN, ACT_NEXT); }
	lv_obj_scroll_to_y(bepio_contacts_ui.list, 0, LV_ANIM_OFF);
}

void bepio_contacts_list_page_render(void)
{
	lv_obj_t *header_right = bepio_page_header_right(bepio_contacts_ui.root);
	bepio_page_set_title(bepio_contacts_ui.root, "Contacts", 40);
	if (header_right) {
		lv_obj_t *add = bepio_contacts_page_button(header_right, LV_SYMBOL_PLUS, 0, 0, 40,
			BEPIO_PAGE_HEADER_HEIGHT, GREEN, ACT_NEW);
		lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, 0);
	} else {
		bepio_contacts_page_text(bepio_contacts_ui.root, "Contacts", 14, 9, 0, INK);
		bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_PLUS, lv_obj_get_width(bepio_contacts_ui.root) - 40, 0, 40, 32, GREEN, ACT_NEW);
	}
	bepio_contacts_ui.list = lv_obj_create(bepio_contacts_ui.root);
	int top = header_right ? 4 : 40;
	lv_obj_set_pos(bepio_contacts_ui.list, 0, top);
	lv_obj_set_size(bepio_contacts_ui.list, lv_obj_get_width(bepio_contacts_ui.root), lv_obj_get_height(bepio_contacts_ui.root) - top);
	lv_obj_set_style_pad_all(bepio_contacts_ui.list, 0, 0);
	lv_obj_set_style_border_width(bepio_contacts_ui.list, 0, 0);
	lv_obj_set_style_bg_color(bepio_contacts_ui.list, lv_color_hex(BG), 0);
	lv_obj_set_scroll_dir(bepio_contacts_ui.list, LV_DIR_VER);
	bepio_contacts_list_page_refresh();
}
