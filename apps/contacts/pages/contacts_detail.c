#include "contacts_detail.h"
#include <stdlib.h>
#include "contacts_page_common.h"
#include "lv_bottom_sheet.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"

static void delete_sheet_closed(lv_event_t *event)
{
	(void)event;
	contacts_controller_cancel_delete(contacts_app_get_controller());
}

static void keep_contact(lv_event_t *event)
{
	contacts_controller_cancel_delete(contacts_app_get_controller());
	lv_bottom_sheet_close(lv_event_get_user_data(event));
}

static void delete_contact(lv_event_t *event)
{
	lv_obj_t *label = lv_event_get_user_data(event);
	int ret = contacts_controller_confirm_delete(contacts_app_get_controller());
	if (ret) {
		lv_label_set_text(label, ret == -EIO ? "Flash save failed. Try again." :
			"Contact could not be deleted.");
	}
}

static lv_obj_t *sheet_button(lv_obj_t *parent, const char *text, uint32_t color)
{
	lv_obj_t *button = lv_button_create(parent);
	lv_obj_set_size(button, 0, 36);
	lv_obj_set_flex_grow(button, 1);
	lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
	lv_obj_set_style_shadow_width(button, 0, 0);
	lv_obj_t *label = lv_label_create(button);
	lv_label_set_text(label, text);
	lv_obj_center(label);
	return button;
}

int bepio_contacts_detail_show_delete_sheet(bepio_contacts_page_context_t *ctx)
{
	contacts_controller_t *controller = contacts_app_get_controller();
	int ret = contacts_controller_request_delete(controller);
	if (ret) { return ret; }
	lv_bottom_sheet_t *sheet = lv_bottom_sheet_create(lv_obj_get_screen(ctx->root));
	if (!sheet) {
		contacts_controller_cancel_delete(controller);
		return -ENOMEM;
	}
	lv_obj_add_event_cb(sheet->overlay, delete_sheet_closed, LV_EVENT_DELETE, NULL);
	lv_bottom_sheet_add_header(sheet, "Delete contact?");
	lv_bottom_sheet_set_height(sheet, 170);
	lv_obj_set_style_radius(sheet->sheet, 12, 0);
	lv_obj_t *content = lv_bottom_sheet_get_content(sheet);
	lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_all(content, 10, 0);
	lv_obj_set_style_pad_row(content, 5, 0);
	lv_obj_t *message = lv_label_create(content);
	lv_label_set_text(message, "Remove this contact from your address book?");
	lv_obj_set_width(message, lv_pct(100));
	lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
	lv_obj_t *error = lv_label_create(content);
	lv_label_set_text(error, "");
	lv_obj_set_style_text_color(error, lv_color_hex(RED), 0);
	lv_obj_t *actions = lv_obj_create(content);
	lv_obj_remove_style_all(actions);
	lv_obj_set_size(actions, lv_pct(100), 36);
	lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
	lv_obj_set_style_pad_column(actions, 8, 0);
	lv_obj_t *keep = sheet_button(actions, "Keep contact", GREEN);
	lv_obj_add_event_cb(keep, keep_contact, LV_EVENT_CLICKED, sheet);
	lv_obj_t *remove = sheet_button(actions, "Delete contact", RED);
	lv_obj_add_event_cb(remove, delete_contact, LV_EVENT_CLICKED, error);
	return 0;
}

static void message_button(bepio_contacts_page_context_t *ctx, int y)
{
	int width = bepio_contacts_page_wide(ctx) ? 148 : 196;
	if (!bepio_contacts_page_wide(ctx)) { y += 48; }
	lv_obj_t *button = bepio_contacts_page_button(ctx, ctx->root, "",
		lv_obj_get_width(ctx->root) - width - 12, y, width, 40, GREEN, ACT_MESSAGE);
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

static void favorite_button(bepio_contacts_page_context_t *ctx, int y, bool favorite)
{
	lv_obj_t *button = bepio_contacts_page_button(ctx, ctx->root, "", 108, y, 40, 40,
		favorite ? 0xc48a20 : MUTED, ACT_FAVORITE);
	lv_obj_clean(button);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_t *icon = lv_image_create(button);
	lv_image_set_src(icon, &ic_bookmark_star_28);
	lv_obj_set_style_image_recolor(icon, lv_color_white(), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_center(icon);
}

static void bepio_contacts_detail_page_render(bepio_contacts_page_context_t *ctx)
{
	lv_page_set_title(ctx->root, "Contact Detail", 40);
	int shift = lv_page_has_header(ctx->root) ? -36 : 0;
	contacts_entry_t entry;
	if (contacts_model_get(ctx->model, ctx->model->flow.selected_id, &entry)) {
		bepio_contacts_page_text(ctx->root, "Contact not found", 22, 80, 200, RED); return;
	}
	if (bepio_contacts_page_wide(ctx)) {
		char glyph[] = {bepio_contacts_page_initial(entry.name), 0};
		lv_obj_t *avatar = lv_obj_create(ctx->root);
		lv_obj_set_pos(avatar, 12, 24); lv_obj_set_size(avatar, 64, 64);
		lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
		lv_obj_set_style_border_width(avatar, 0, 0);
		lv_obj_center(bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN));
		lv_obj_t *name = bepio_contacts_page_text(ctx->root, entry.name, 92, 12, 216, INK);
		lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
		lv_obj_t *phone = bepio_contacts_page_text(ctx->root, entry.phone, 92, 48, 216, INK);
		lv_obj_update_layout(ctx->root);
		int name_height = lv_obj_get_height(name);
		int info_height = name_height + 12 + lv_obj_get_height(phone);
		int info_y = lv_obj_get_y(avatar) + (lv_obj_get_height(avatar) - info_height) / 2;
		lv_obj_set_y(name, info_y);
		lv_obj_set_y(phone, info_y + name_height + 12);
		bepio_contacts_page_button(ctx, ctx->root, LV_SYMBOL_TRASH, 12, 118, 40, 38, RED, ACT_DELETE);
		bepio_contacts_page_button(ctx, ctx->root, LV_SYMBOL_EDIT, 60, 118, 40, 38, GREEN, ACT_EDIT);
		message_button(ctx, 118);
		favorite_button(ctx, 118, entry.favorite);
		ctx->error = bepio_contacts_page_text(ctx->root, "", 12, 94, 296, RED);
		return;
	}
	if (!lv_page_has_header(ctx->root)) { bepio_contacts_page_button(ctx, ctx->root, LV_SYMBOL_LEFT " Contacts", 12, 8, 115, 30, GREEN, ACT_BACK); }
	char glyph[] = {bepio_contacts_page_initial(entry.name), 0};
	lv_obj_t *avatar = lv_obj_create(ctx->root);
	lv_obj_set_pos(avatar, 86, 50 + shift); lv_obj_set_size(avatar, 68, 68);
	lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
	lv_obj_set_style_border_width(avatar, 0, 0);
	lv_obj_t *letter = bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN);
	lv_obj_set_style_text_font(letter, &lv_font_montserrat_20, 0); lv_obj_center(letter);
	lv_obj_t *name = bepio_contacts_page_text(ctx->root, entry.name, 12, 128 + shift, 216, INK);
	lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
	bepio_contacts_page_text(ctx->root, entry.phone, 18, 164 + shift, 210, INK);
	bepio_contacts_page_button(ctx, ctx->root, LV_SYMBOL_TRASH, 12, 224 + shift, 40, 38, RED, ACT_DELETE);
	bepio_contacts_page_button(ctx, ctx->root, LV_SYMBOL_EDIT, 60, 224 + shift, 40, 38, GREEN, ACT_EDIT);
	message_button(ctx, 224 + shift);
	favorite_button(ctx, 224 + shift, entry.favorite);
	ctx->error = bepio_contacts_page_text(ctx->root, "", 12, 206 + shift, 216, RED);
}

lv_obj_t *bepio_contacts_detail_build(void *app, void *payload)
{
	(void)payload;
	contacts_view_t *view = app;
	bepio_contacts_page_context_t *ctx = bepio_contacts_page_alloc(sizeof(bepio_contacts_page_context_t), view, view->model);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = bepio_contacts_page_begin(ctx, "Contact Detail");
	if (!screen) { free(ctx); return NULL; }
	ctx->rebuilding = true;
	bepio_contacts_detail_page_render(ctx);
	ctx->rebuilding = false;
	return bepio_contacts_page_finish(ctx);
}
