#include "contacts_discard_confirm.h"
#include "contacts_page_common.h"

void bepio_contacts_discard_confirm_page_render(void)
{
	bepio_page_set_title(bepio_contacts_ui.root, "Discard Changes", 40);
	if (bepio_contacts_page_wide()) {
		lv_obj_t *title = bepio_contacts_page_text(bepio_contacts_ui.root, "Discard changes?", 12, 8, 296, INK);
		lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
		bepio_contacts_page_text(bepio_contacts_ui.root, "Your unsaved changes will be lost.", 12, 44, 296, MUTED);
		bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 12, 86, 296, RED);
		bepio_contacts_page_button(bepio_contacts_ui.root, "Keep editing", 12, 118, 144, 38, GREEN, ACT_BACK);
		bepio_contacts_page_button(bepio_contacts_ui.root, "Discard changes", 164, 118, 144, 38, RED,
			ACT_DISCARD);
		return;
	}
	int shift = bepio_page_has_header(bepio_contacts_ui.root) ? -36 : 0;
	lv_obj_t *title = bepio_contacts_page_text(bepio_contacts_ui.root, "Discard changes?", 16, 42 + shift, 208, INK);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
	bepio_contacts_page_text(bepio_contacts_ui.root, "Your unsaved changes will be lost.", 16, 88 + shift, 208, MUTED);
	bepio_contacts_ui.error = bepio_contacts_page_text(bepio_contacts_ui.root, "", 16, 147 + shift, 208, RED);
	bepio_contacts_page_button(bepio_contacts_ui.root, "Keep editing", 16, 178 + shift, 208, 38, GREEN, ACT_BACK);
	bepio_contacts_page_button(bepio_contacts_ui.root, "Discard changes", 16, 226 + shift, 208, 38, RED,
		ACT_DISCARD);
}
