#include "contacts_detail.h"
#include "contacts_page_common.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"

static void message_button(int y)
{
	int width = bepio_contacts_page_wide() ? 148 : 196;
	if (!bepio_contacts_page_wide()) { y += 48; }
	lv_obj_t *button = bepio_contacts_page_button(bepio_contacts_ui.root, "",
		lv_obj_get_width(bepio_contacts_ui.root) - width - 12, y, width, 40, GREEN, ACT_MESSAGE);
	lv_obj_clean(button);
	lv_obj_t *icon = lv_image_create(button);
	lv_image_set_src(icon, &ic_send_28);
	lv_obj_set_style_image_recolor(icon, lv_color_hex(0xffffff), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
	lv_obj_t *text = bepio_contacts_page_text(button, "Send Message", 0, 0, 0, 0xffffff);
	lv_label_set_long_mode(text, LV_LABEL_LONG_CLIP);
	lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(text, LV_ALIGN_RIGHT_MID, -8, 0);
}

static void favorite_button(int y, bool favorite)
{
	lv_obj_t *button = bepio_contacts_page_button(bepio_contacts_ui.root, "", 108, y, 40, 40,
		favorite ? 0xc48a20 : MUTED, ACT_FAVORITE);
	lv_obj_clean(button);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_t *icon = lv_image_create(button);
	lv_image_set_src(icon, &ic_bookmark_star_28);
	lv_obj_set_style_image_recolor(icon, lv_color_white(), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_center(icon);
}

void bepio_contacts_detail_page_render(void)
{
	bepio_page_set_title(bepio_contacts_ui.root, "Contact Detail", 40);
	int shift = bepio_page_has_header(bepio_contacts_ui.root) ? -36 : 0;
	contacts_entry_t entry;
	if (contacts_model_get(bepio_contacts_ui.model, bepio_contacts_ui.view->selected_id, &entry)) {
		bepio_contacts_page_text(bepio_contacts_ui.root, "Contact not found", 22, 80, 200, RED); return;
	}
	if (bepio_contacts_page_wide()) {
		char glyph[] = {bepio_contacts_page_initial(entry.name), 0};
		lv_obj_t *avatar = lv_obj_create(bepio_contacts_ui.root);
		lv_obj_set_pos(avatar, 12, 24); lv_obj_set_size(avatar, 64, 64);
		lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
		lv_obj_set_style_border_width(avatar, 0, 0);
		lv_obj_center(bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN));
		lv_obj_t *name = bepio_contacts_page_text(bepio_contacts_ui.root, entry.name, 92, 12, 216, INK);
		lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
		lv_obj_t *phone = bepio_contacts_page_text(bepio_contacts_ui.root, entry.phone, 92, 48, 216, INK);
		lv_obj_update_layout(bepio_contacts_ui.root);
		int name_height = lv_obj_get_height(name);
		int info_height = name_height + 12 + lv_obj_get_height(phone);
		int info_y = lv_obj_get_y(avatar) + (lv_obj_get_height(avatar) - info_height) / 2;
		lv_obj_set_y(name, info_y);
		lv_obj_set_y(phone, info_y + name_height + 12);
		bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_TRASH, 12, 118, 40, 38, RED, ACT_DELETE);
		bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_EDIT, 60, 118, 40, 38, GREEN, ACT_EDIT);
		message_button(118);
		favorite_button(118, entry.favorite);
		bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 12, 94, 296, RED);
		return;
	}
	if (!bepio_page_has_header(bepio_contacts_ui.root)) { bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_LEFT " Contacts", 12, 8, 115, 30, GREEN, ACT_BACK); }
	char glyph[] = {bepio_contacts_page_initial(entry.name), 0};
	lv_obj_t *avatar = lv_obj_create(bepio_contacts_ui.root);
	lv_obj_set_pos(avatar, 86, 50 + shift); lv_obj_set_size(avatar, 68, 68);
	lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
	lv_obj_set_style_border_width(avatar, 0, 0);
	lv_obj_t *letter = bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN);
	lv_obj_set_style_text_font(letter, &lv_font_montserrat_20, 0); lv_obj_center(letter);
	lv_obj_t *name = bepio_contacts_page_text(bepio_contacts_ui.root, entry.name, 12, 128 + shift, 216, INK);
	lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
	bepio_contacts_page_text(bepio_contacts_ui.root, entry.phone, 18, 164 + shift, 210, INK);
	bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_TRASH, 12, 224 + shift, 40, 38, RED, ACT_DELETE);
	bepio_contacts_page_button(bepio_contacts_ui.root, LV_SYMBOL_EDIT, 60, 224 + shift, 40, 38, GREEN, ACT_EDIT);
	message_button(224 + shift);
	favorite_button(224 + shift, entry.favorite);
	bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 12, 206 + shift, 216, RED);
}
