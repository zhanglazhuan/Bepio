/* Standard page skeleton inherited from DeepStoa. SPDX-License-Identifier: GPL-3.0-only */
#include "lv_page.h"

static lv_obj_t *page_header(const lv_obj_t *content)
{
	if (!content) { return NULL; }
	lv_obj_t *screen = lv_obj_get_parent(content);
	if (!screen || lv_obj_get_child_count(screen) < 3 ||
		lv_obj_get_child(screen, 2) != content) { return NULL; }
	lv_obj_t *header = lv_obj_get_child(screen, 1);
	return lv_obj_get_child_count(header) == 3 ? header : NULL;
}

void lv_page_set_title(const lv_obj_t *content, const char *title, int right_width)
{
	lv_obj_t *header = page_header(content);
	if (!header) { return; }
	lv_obj_t *heading = lv_obj_get_child(header, 1);
	if (lv_obj_check_type(heading, &lv_label_class)) {
		lv_label_set_text(heading, title ? title : "");
	}
	lv_obj_set_width(lv_obj_get_child(header, 2), right_width);
}

lv_obj_t *lv_page_header_right(const lv_obj_t *content)
{
	lv_obj_t *header = page_header(content);
	return header ? lv_obj_get_child(header, 2) : NULL;
}

bool lv_page_has_header(const lv_obj_t *content)
{
	return page_header(content) != NULL;
}

Page lv_page_create(const char *title, bool allow_back,
	lv_event_cb_t back_cb, void *user_data)
{
	Page page = {0};
	page.screen = lv_obj_create(NULL);
	if (!page.screen) { return page; }
	lv_obj_set_size(page.screen, lv_display_get_horizontal_resolution(NULL),
		lv_display_get_vertical_resolution(NULL));
	lv_obj_set_flex_flow(page.screen, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_all(page.screen, 0, 0);
	lv_obj_set_style_pad_row(page.screen, 0, 0);
	lv_obj_set_scrollbar_mode(page.screen, LV_SCROLLBAR_MODE_OFF);
	lv_obj_remove_flag(page.screen, LV_OBJ_FLAG_SCROLLABLE);

	/* The global status bar lives on lv_layer_top; each screen reserves its space. */
	lv_obj_t *spacer = lv_obj_create(page.screen);
	lv_obj_remove_style_all(spacer);
	lv_obj_set_size(spacer, LV_PCT(100), LV_STATUS_BAR_HEIGHT);

	page.header = lv_obj_create(page.screen);
	lv_obj_remove_style_all(page.header);
	lv_obj_set_style_text_color(page.header, lv_color_black(), 0);
	lv_obj_set_size(page.header, LV_PCT(100), LV_PAGE_HEADER_HEIGHT);
	lv_obj_set_flex_flow(page.header, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(page.header, LV_FLEX_ALIGN_SPACE_BETWEEN,
		LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_scroll_dir(page.header, LV_DIR_NONE);

	lv_obj_t *back;
	if (allow_back) {
		back = lv_button_create(page.header);
		lv_obj_set_size(back, 40, LV_PAGE_HEADER_HEIGHT);
		lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
		lv_obj_set_style_shadow_width(back, 0, 0);
		lv_obj_set_style_border_width(back, 0, 0);
		lv_obj_set_style_pad_all(back, 0, 0);
		lv_obj_t *icon = lv_label_create(back);
		lv_label_set_text(icon, LV_SYMBOL_LEFT);
		lv_obj_set_style_text_color(icon, lv_color_black(), 0);
		lv_obj_center(icon);
		if (back_cb) { lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, user_data); }
	} else {
		back = lv_obj_create(page.header);
		lv_obj_remove_style_all(back);
		lv_obj_set_size(back, 40, LV_PAGE_HEADER_HEIGHT);
	}

	lv_obj_t *heading;
	if (title) {
		heading = lv_label_create(page.header);
		lv_label_set_text(heading, title);
		lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT);
		lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
	} else {
		heading = lv_obj_create(page.header);
		lv_obj_remove_style_all(heading);
	}
	lv_obj_set_flex_grow(heading, 1);

	page.header_right = lv_obj_create(page.header);
	lv_obj_remove_style_all(page.header_right);
	lv_obj_set_style_text_color(page.header_right, lv_color_black(), 0);
	lv_obj_set_size(page.header_right, 40, LV_PAGE_HEADER_HEIGHT);

	page.container = lv_obj_create(page.screen);
	lv_obj_set_width(page.container, LV_PCT(100));
	lv_obj_set_flex_grow(page.container, 1);
	lv_obj_set_flex_flow(page.container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_border_width(page.container, 0, 0);
	lv_obj_set_style_radius(page.container, 0, 0);
	lv_obj_set_style_pad_all(page.container, LV_PAGE_HOR_PAD, 0);
	lv_obj_set_scrollbar_mode(page.container, LV_SCROLLBAR_MODE_OFF);
	lv_obj_update_layout(page.screen);
	return page;
}
