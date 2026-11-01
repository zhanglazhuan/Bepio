#include "contacts_edit.h"
#include <stdlib.h>
#include "contacts_page_common.h"
#include "lv_bottom_sheet.h"
typedef struct {
	bepio_contacts_page_context_t base;
	lv_obj_t *keyboard, *name, *phone;
} edit_context_t;

int bepio_contacts_edit_page_sync(bepio_contacts_page_context_t *base)
{
	edit_context_t *ctx = (edit_context_t *)base;
	if (!ctx->name || !ctx->phone) { return -EINVAL; }
	return contacts_controller_draft(contacts_app_get_controller(),
		lv_textarea_get_text(ctx->name), lv_textarea_get_text(ctx->phone));
}

static void discard_sheet_closed(lv_event_t *event)
{
	(void)event;
	contacts_controller_cancel_discard(contacts_app_get_controller());
}

static void keep_editing(lv_event_t *event)
{
	contacts_controller_cancel_discard(contacts_app_get_controller());
	lv_bottom_sheet_close(lv_event_get_user_data(event));
}

static void discard_changes(lv_event_t *event)
{
	lv_obj_t *error = lv_event_get_user_data(event);
	int ret = contacts_controller_discard(contacts_app_get_controller());
	if (ret) { lv_label_set_text(error, "Could not discard changes."); }
}

static lv_obj_t *sheet_button(lv_obj_t *parent, const char *text, uint32_t color)
{
	lv_obj_t *button = lv_button_create(parent);
	lv_obj_set_size(button, lv_pct(100), 36);
	lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
	lv_obj_set_style_shadow_width(button, 0, 0);
	lv_obj_t *label = lv_label_create(button);
	lv_label_set_text(label, text);
	lv_obj_center(label);
	return button;
}

static int show_discard_sheet(edit_context_t *ctx)
{
	lv_bottom_sheet_t *sheet = lv_bottom_sheet_create(lv_obj_get_screen(ctx->base.root));
	if (!sheet) { return -ENOMEM; }
	lv_obj_add_event_cb(sheet->overlay, discard_sheet_closed, LV_EVENT_DELETE, NULL);
	lv_bottom_sheet_add_header(sheet, "Discard changes?");
	lv_bottom_sheet_set_height(sheet, 206);
	lv_obj_set_style_radius(sheet->sheet, 12, 0);
	lv_obj_t *content = lv_bottom_sheet_get_content(sheet);
	lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_all(content, 10, 0);
	lv_obj_set_style_pad_row(content, 5, 0);
	lv_obj_t *message = lv_label_create(content);
	lv_label_set_text(message, "Your unsaved changes will be lost.");
	lv_obj_set_width(message, lv_pct(100));
	lv_obj_t *error = lv_label_create(content);
	lv_label_set_text(error, "");
	lv_obj_set_style_text_color(error, lv_color_hex(RED), 0);
	lv_obj_t *keep = sheet_button(content, "Keep editing", GREEN);
	lv_obj_add_event_cb(keep, keep_editing, LV_EVENT_CLICKED, sheet);
	lv_obj_t *discard = sheet_button(content, "Discard changes", RED);
	lv_obj_add_event_cb(discard, discard_changes, LV_EVENT_CLICKED, error);
	return 0;
}

void bepio_contacts_edit_request_back(bepio_contacts_page_context_t *base)
{
	contacts_model_t *model = base->model;
	if (model->flow.discard_pending) { return; }
	int ret = bepio_contacts_edit_page_sync(base);
	if (ret) { bepio_contacts_page_report(base, ret); return; }
	contacts_controller_back(contacts_app_get_controller());
	if (model->flow.discard_pending) {
		ret = show_discard_sheet((edit_context_t *)base);
		if (ret) {
			contacts_controller_cancel_discard(contacts_app_get_controller());
			bepio_contacts_page_report(base, ret);
		}
	}
}

static void keyboard_event(lv_event_t *event)
{
	edit_context_t *ctx = lv_event_get_user_data(event);
	if (!ctx->base.view || ctx->base.rebuilding) { return; }
	lv_event_code_t code = lv_event_get_code(event);
	if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) { return; }
	if (code == LV_EVENT_CANCEL) { bepio_contacts_edit_request_back(&ctx->base); }
	else {
		int ret = bepio_contacts_edit_page_sync(&ctx->base);
		if (!ret) { ret = contacts_controller_save(contacts_app_get_controller()); }
		bepio_contacts_page_report(&ctx->base, ret);
	}
}
static lv_obj_t *keyboard(edit_context_t *ctx)
{
	lv_obj_t *obj = lv_keyboard_create(ctx->base.root);

	lv_obj_set_align(obj, LV_ALIGN_TOP_LEFT);
	if (bepio_contacts_page_wide(&ctx->base)) {
		lv_obj_set_pos(obj, 0, lv_obj_get_height(ctx->base.root) - 84);
		lv_obj_set_size(obj, lv_obj_get_width(ctx->base.root), 84);
	} else if (lv_page_has_header(ctx->base.root)) {
		lv_obj_set_pos(obj, 0, lv_obj_get_height(ctx->base.root) - 96);
		lv_obj_set_size(obj, 240, 96);
	} else { lv_obj_set_pos(obj, 0, 164); lv_obj_set_size(obj, 240, 116); }
	lv_obj_set_style_pad_all(obj, 2, 0);
	lv_obj_set_style_pad_row(obj, 3, 0);
	lv_obj_set_style_pad_column(obj, 3, 0);
	lv_obj_add_event_cb(obj, keyboard_event, LV_EVENT_READY, ctx);
	lv_obj_add_event_cb(obj, keyboard_event, LV_EVENT_CANCEL, ctx);
	return obj;
}

static void form_event(lv_event_t *event)
{
	edit_context_t *ctx = lv_event_get_user_data(event);
	if (!ctx->base.view || ctx->base.rebuilding) { return; }
	lv_obj_t *target = lv_event_get_target(event);
	if (lv_event_get_code(event) == LV_EVENT_FOCUSED || lv_event_get_code(event) == LV_EVENT_CLICKED) {
		lv_keyboard_set_mode(ctx->keyboard, target == ctx->phone ?
			LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
		lv_keyboard_set_textarea(ctx->keyboard, target);
	} else if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
		int ret = bepio_contacts_edit_page_sync(&ctx->base);
		if (ret) { bepio_contacts_page_report(&ctx->base, ret); }
		else { lv_label_set_text(ctx->base.error, ""); }
	}
}

static lv_obj_t *field(edit_context_t *ctx, const char *placeholder,
	const char *value, int y, unsigned maximum)
{
	lv_obj_t *obj = lv_textarea_create(ctx->base.root);
	lv_obj_set_pos(obj, 12, y); lv_obj_set_size(obj, lv_obj_get_width(ctx->base.root) - 24, 36);
	lv_textarea_set_one_line(obj, true);
	lv_textarea_set_max_length(obj, maximum);
	lv_textarea_set_placeholder_text(obj, placeholder);
	lv_textarea_set_text(obj, value);
	lv_obj_set_style_pad_all(obj, 8, 0);
	lv_obj_set_style_radius(obj, 9, 0);
	lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
	lv_obj_set_style_text_color(obj, lv_color_hex(INK), 0);
	return obj;
}

static void bepio_contacts_edit_page_render(bepio_contacts_page_context_t *base)
{
	edit_context_t *ctx = (edit_context_t *)base;
	lv_page_set_title(ctx->base.root, ctx->base.model->flow.draft.id ? "Edit Contact" : "New Contact", 40);
	bool compact = lv_page_has_header(ctx->base.root);
	if (bepio_contacts_page_wide(&ctx->base)) {
		ctx->base.error = bepio_contacts_page_text(ctx->base.root, "Name and mobile number", 8, 0, 304, MUTED);
		ctx->name = field(ctx, "Name", ctx->base.model->flow.draft.name, 20, CONTACTS_NAME_MAX);
		ctx->phone = field(ctx, "Device ID / phone", ctx->base.model->flow.draft.phone, 20, CONTACTS_PHONE_MAX);
		lv_obj_set_pos(ctx->name, 8, 20); lv_obj_set_size(ctx->name, 148, 32);
		lv_obj_set_pos(ctx->phone, 164, 20); lv_obj_set_size(ctx->phone, 148, 32);
		lv_obj_set_style_pad_ver(ctx->name, 6, 0); lv_obj_set_style_pad_ver(ctx->phone, 6, 0);
		lv_textarea_set_accepted_chars(ctx->phone, "0123456789");
		bepio_contacts_page_button(&ctx->base, ctx->base.root, "Cancel", 8, 57, 148, 27, MUTED, ACT_BACK);
		bepio_contacts_page_button(&ctx->base, ctx->base.root, "Save", 164, 57, 148, 27, GREEN, ACT_SAVE);
		ctx->keyboard = keyboard(ctx); lv_keyboard_set_textarea(ctx->keyboard, ctx->name);
		lv_obj_add_event_cb(ctx->name, form_event, LV_EVENT_ALL, ctx);
		lv_obj_add_event_cb(ctx->phone, form_event, LV_EVENT_ALL, ctx);
		return;
	}
	lv_obj_t *title = bepio_contacts_page_text(ctx->base.root, ctx->base.model->flow.draft.id ? "Edit contact" : "New contact", 12, compact ? 0 : 7, 0, INK);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
	ctx->base.error = bepio_contacts_page_text(ctx->base.root, "", 12, compact ? 23 : 29, 216, RED);
	ctx->name = field(ctx, "Name", ctx->base.model->flow.draft.name, compact ? 40 : 46, CONTACTS_NAME_MAX);
	ctx->phone = field(ctx, "Device ID / phone", ctx->base.model->flow.draft.phone, compact ? 72 : 88, CONTACTS_PHONE_MAX);
	if (compact) {
		lv_obj_set_height(ctx->name, 30); lv_obj_set_height(ctx->phone, 30);
		lv_obj_set_style_pad_ver(ctx->name, 6, 0); lv_obj_set_style_pad_ver(ctx->phone, 6, 0);
	}
	lv_textarea_set_accepted_chars(ctx->phone, "0123456789");
	bepio_contacts_page_button(&ctx->base, ctx->base.root, "Cancel", 12, compact ? 104 : 130, 100, compact ? 28 : 30, MUTED, ACT_BACK);
	bepio_contacts_page_button(&ctx->base, ctx->base.root, "Save", 124, compact ? 104 : 130, 104, compact ? 28 : 30, GREEN, ACT_SAVE);
	ctx->keyboard = keyboard(ctx);
	lv_keyboard_set_textarea(ctx->keyboard, ctx->name);
	lv_obj_add_event_cb(ctx->name, form_event, LV_EVENT_ALL, ctx);
	lv_obj_add_event_cb(ctx->phone, form_event, LV_EVENT_ALL, ctx);
}

lv_obj_t *bepio_contacts_edit_build(void *app, void *payload)
{
	(void)payload;
	contacts_view_t *view = app;
	bepio_contacts_page_context_t *ctx = bepio_contacts_page_alloc(sizeof(edit_context_t), view, view->model);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = bepio_contacts_page_begin(ctx, "New Contact");
	if (!screen) { free(ctx); return NULL; }
	ctx->rebuilding = true;
	bepio_contacts_edit_page_render(ctx);
	ctx->rebuilding = false;
	return bepio_contacts_page_finish(ctx);
}
