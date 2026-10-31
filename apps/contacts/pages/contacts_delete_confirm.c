#include "contacts_delete_confirm.h"
#include "contacts_page_common.h"

void bepio_contacts_delete_confirm_page_render(void)
{
	bepio_page_set_title(bepio_contacts_ui.root, "Delete Contact", 40);
	if (bepio_contacts_page_wide()) {
		lv_obj_t *title = bepio_contacts_page_text(bepio_contacts_ui.root, "Delete contact?", 12, 8, 296, INK);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
		bepio_contacts_page_text(bepio_contacts_ui.root, "Remove this contact from your address book?", 12, 44, 296, MUTED);
		bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 12, 86, 296, RED);
		bepio_contacts_page_button(bepio_contacts_ui.root, "Keep contact", 12, 118, 144, 38, GREEN, ACT_BACK);
		bepio_contacts_page_button(bepio_contacts_ui.root, "Delete contact", 164, 118, 144, 38, RED,
			ACT_DELETE_YES);
		return;
	}
	int shift = bepio_page_has_header(bepio_contacts_ui.root) ? -36 : 0;
	lv_obj_t *title = bepio_contacts_page_text(bepio_contacts_ui.root, "Delete contact?", 16, 42 + shift, 208, INK);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
	bepio_contacts_page_text(bepio_contacts_ui.root, "This contact will be removed from your address book.", 16, 88 + shift, 208, MUTED);
	bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 16, 147 + shift, 208, RED);
	bepio_contacts_page_button(bepio_contacts_ui.root, "Keep contact", 16, 178 + shift, 208, 38, GREEN, ACT_BACK);
	bepio_contacts_page_button(bepio_contacts_ui.root, "Delete contact", 16, 226 + shift, 208, 38, RED,
		ACT_DELETE_YES);
}
