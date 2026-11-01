#include "contacts_list.h"
#include <stdlib.h>
#include "contacts_page_common.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"
typedef struct {
	bepio_contacts_page_context_t base;
	lv_obj_t *list;
} list_context_t;

static void open_contact(lv_event_t *event)
{
	bepio_contacts_page_context_t *ctx = lv_event_get_user_data(event);
	if (!ctx->view) { return; }
	uint32_t id = (uint32_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target_obj(event));
	contacts_entry_t entry;
	bepio_contacts_page_report(ctx, contacts_controller_show(contacts_app_get_controller(), id, &entry));
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

static void bepio_contacts_list_page_refresh(bepio_contacts_page_context_t *base)
{
	list_context_t *ctx = (list_context_t *)base;
	lv_obj_clean(ctx->list);
	size_t indexes[CONFIG_BEPIO_CONTACTS_CAPACITY], count = 0;
	for (size_t i = 0; i < ctx->base.model->count; i++) {
		{
			size_t at = count;
			while (at && name_compare(ctx->base.model->entries[indexes[at - 1]].name, ctx->base.model->entries[i].name) > 0) {
				indexes[at] = indexes[at - 1]; at--;
			}
			indexes[at] = i; count++;
		}
	}
	if (!count) {
		lv_obj_t *empty = bepio_contacts_page_text(ctx->list, "No contact yet, click + goto add",
			8, 30, lv_obj_get_width(ctx->base.root) - 16, MUTED);
		lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
		return;
	}
	if (ctx->base.model->flow.list_offset >= count) { ctx->base.model->flow.list_offset = 0; }
	int y = 0;
	char previous = 0;
	size_t end = ctx->base.model->flow.list_offset + PAGE_SIZE < count ? ctx->base.model->flow.list_offset + PAGE_SIZE : count;
	for (size_t i = ctx->base.model->flow.list_offset; i < end; i++) {
		const contacts_entry_t *entry = &ctx->base.model->entries[indexes[i]];
		char letter = bepio_contacts_page_initial(entry->name), glyph[] = {letter, 0};
		if (!bepio_contacts_page_wide(&ctx->base) && letter != previous) { bepio_contacts_page_text(ctx->list, glyph, 15, y + 3, 0, GREEN); y += 24; previous = letter; }
		lv_obj_t *row = lv_button_create(ctx->list);
		lv_obj_set_pos(row, bepio_contacts_page_wide(&ctx->base) ? 8 : 12, y);
		lv_obj_set_size(row, lv_obj_get_width(ctx->base.root) - (bepio_contacts_page_wide(&ctx->base) ? 16 : 24), bepio_contacts_page_wide(&ctx->base) ? 44 : 58);
		lv_obj_set_style_bg_color(row, lv_color_white(), 0);
		lv_obj_set_style_radius(row, 12, 0);
		lv_obj_set_style_shadow_width(row, 0, 0);
		lv_obj_set_style_pad_all(row, 0, 0);
		lv_obj_set_user_data(row, (void *)(uintptr_t)entry->id);
		lv_obj_add_event_cb(row, open_contact, LV_EVENT_CLICKED, &ctx->base);
		lv_obj_t *avatar = lv_obj_create(row);
		lv_obj_set_pos(avatar, 10, bepio_contacts_page_wide(&ctx->base) ? 5 : 12); lv_obj_set_size(avatar, 34, 34);
		lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(avatar, lv_color_hex(0xe2f0e9), 0);
		lv_obj_set_style_border_width(avatar, 0, 0);
		lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_center(bepio_contacts_page_text(avatar, glyph, 0, 0, 0, GREEN));
		lv_obj_t *name = bepio_contacts_page_text(row, entry->name, 54, bepio_contacts_page_wide(&ctx->base) ? 13 : 8, bepio_contacts_page_wide(&ctx->base) ? 116 : (entry->favorite ? 126 : 156), INK);
		lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
		lv_obj_set_height(name, lv_font_get_line_height(lv_obj_get_style_text_font(name, 0)));
		lv_obj_t *phone = bepio_contacts_page_text(row, entry->phone, bepio_contacts_page_wide(&ctx->base) ? 174 : 54, bepio_contacts_page_wide(&ctx->base) ? 13 : 32, bepio_contacts_page_wide(&ctx->base) ? (entry->favorite ? 88 : 124) : (entry->favorite ? 128 : 156), MUTED);
		lv_label_set_long_mode(phone, LV_LABEL_LONG_DOT);
		lv_obj_set_height(phone, lv_font_get_line_height(lv_obj_get_style_text_font(phone, 0)));
		if (entry->favorite) {
			lv_obj_t *icon = lv_image_create(row);
			lv_image_set_src(icon, &ic_bookmark_star_28);
			lv_obj_set_style_image_recolor(icon, lv_color_hex(0xc48a20), 0);
			lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
			lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -8, 0);
		}
		y += bepio_contacts_page_wide(&ctx->base) ? 48 : 64;
	}
	if (ctx->base.model->flow.list_offset) { bepio_contacts_page_button(&ctx->base, ctx->list, "Previous", 12, y + 8, 100, 34, MUTED, ACT_PREVIOUS); }
	if (end < count) { bepio_contacts_page_button(&ctx->base, ctx->list, "More", 124, y + 8, 104, 34, GREEN, ACT_NEXT); }
	lv_obj_scroll_to_y(ctx->list, 0, LV_ANIM_OFF);
}

void bepio_contacts_list_page_advance(bepio_contacts_page_context_t *base, int pages)
{
	list_context_t *ctx = (list_context_t *)base;
	if (pages > 0) { ctx->base.model->flow.list_offset += PAGE_SIZE; }
	else { ctx->base.model->flow.list_offset = ctx->base.model->flow.list_offset >= PAGE_SIZE ? ctx->base.model->flow.list_offset - PAGE_SIZE : 0; }
	bepio_contacts_list_page_refresh(base);
}

static void bepio_contacts_list_page_render(bepio_contacts_page_context_t *base)
{
	list_context_t *ctx = (list_context_t *)base;
	lv_obj_t *header_right = lv_page_header_right(ctx->base.root);
	lv_page_set_title(ctx->base.root, "Contacts", 40);
	if (header_right) {
		lv_obj_t *add = bepio_contacts_page_button(&ctx->base, header_right, LV_SYMBOL_PLUS, 0, 0, 40,
			LV_PAGE_HEADER_HEIGHT, GREEN, ACT_NEW);
		lv_obj_set_style_bg_opa(add, LV_OPA_TRANSP, 0);
	} else {
		bepio_contacts_page_text(ctx->base.root, "Contacts", 14, 9, 0, INK);
		bepio_contacts_page_button(&ctx->base, ctx->base.root, LV_SYMBOL_PLUS, lv_obj_get_width(ctx->base.root) - 40, 0, 40, 32, GREEN, ACT_NEW);
	}
	ctx->list = lv_obj_create(ctx->base.root);
	int top = header_right ? 4 : 40;
	lv_obj_set_pos(ctx->list, 0, top);
	lv_obj_set_size(ctx->list, lv_obj_get_width(ctx->base.root), lv_obj_get_height(ctx->base.root) - top);
	lv_obj_set_style_pad_all(ctx->list, 0, 0);
	lv_obj_set_style_border_width(ctx->list, 0, 0);
	lv_obj_set_style_bg_color(ctx->list, lv_color_hex(BG), 0);
	lv_obj_set_scroll_dir(ctx->list, LV_DIR_VER);
	bepio_contacts_list_page_refresh(base);
}

lv_obj_t *bepio_contacts_list_build(void *app, void *payload)
{
	(void)payload;
	contacts_view_t *view = app;
	bepio_contacts_page_context_t *ctx = bepio_contacts_page_alloc(sizeof(list_context_t), view, view->model);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = bepio_contacts_page_begin(ctx, "Contacts");
	if (!screen) { free(ctx); return NULL; }
	ctx->rebuilding = true;
	bepio_contacts_list_page_render(ctx);
	ctx->rebuilding = false;
	return bepio_contacts_page_finish(ctx);
}
