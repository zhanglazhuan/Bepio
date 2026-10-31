/* Adapted from DeepStoa/EPOS lv_page. SPDX-License-Identifier: GPL-3.0-only */
#include "lv_page.h"
#include <errno.h>
static lv_obj_t *current_content, *current_header_right, *current_heading;
static bepio_page_message_cb_t message_handler;
void bepio_page_set_message_handler(bepio_page_message_cb_t handler) { message_handler = handler; }
int bepio_page_compose_message(const char *phone, const char *name)
{
	return message_handler ? message_handler(phone, name) : -ENOSYS;
}
void bepio_page_set_title(const lv_obj_t *content, const char *title, int right_width)
{
	if (!content || content != current_content) { return; }
	lv_label_set_text(current_heading, title);
	lv_obj_set_width(current_header_right, right_width);
	lv_obj_set_width(current_heading, lv_obj_get_width(lv_obj_get_parent(content)) - 40 - right_width);
	lv_obj_set_style_pad_hor(current_heading, 0, 0);
	lv_obj_align(current_heading, LV_ALIGN_LEFT_MID, 40, 0);
}
static void content_deleted(lv_event_t *event)
{
	if (lv_event_get_target_obj(event) == current_content) { current_content = current_header_right = current_heading = NULL; }
}
lv_obj_t *bepio_page_header_right(const lv_obj_t *content)
{
	return content && content == current_content ? current_header_right : NULL;
}
bool bepio_page_has_header(const lv_obj_t *content) { return content && content == current_content; }
bepio_page_t bepio_page_create(const char *title, bool allow_back,
			      lv_event_cb_t back_cb, void *user_data)
{
	bepio_page_t page = {0};
	bepio_status_bar_init();
	page.screen = lv_obj_create(NULL);
	lv_obj_remove_style_all(page.screen);
	lv_obj_set_size(page.screen, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
	lv_obj_set_style_bg_color(page.screen, lv_color_hex(0xc7d5b8), 0);
	lv_obj_set_style_bg_opa(page.screen, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(page.screen, lv_color_hex(0x21392a), 0);
	lv_obj_remove_flag(page.screen, LV_OBJ_FLAG_SCROLLABLE);
	page.header = lv_obj_create(page.screen);
	lv_obj_remove_style_all(page.header);
	lv_obj_set_pos(page.header, 0, BEPIO_STATUS_BAR_HEIGHT);
	lv_obj_set_size(page.header, lv_pct(100), BEPIO_PAGE_HEADER_HEIGHT);
	lv_obj_remove_flag(page.header, LV_OBJ_FLAG_SCROLLABLE);
	page.back = lv_button_create(page.header);
	lv_obj_set_pos(page.back, 0, 0);
	lv_obj_set_size(page.back, 40, BEPIO_PAGE_HEADER_HEIGHT);
	lv_obj_set_style_bg_opa(page.back, LV_OPA_TRANSP, 0);
	lv_obj_set_style_shadow_width(page.back, 0, 0);
	lv_obj_t *icon = lv_label_create(page.back);
	lv_label_set_text(icon, LV_SYMBOL_LEFT);
	lv_obj_center(icon);
	if (!allow_back) { lv_obj_add_flag(page.back, LV_OBJ_FLAG_HIDDEN); }
	if (back_cb) { lv_obj_add_event_cb(page.back, back_cb, LV_EVENT_CLICKED, user_data); }
	lv_obj_t *heading = lv_label_create(page.header);
	lv_label_set_text(heading, title ? title : "");
	lv_obj_set_width(heading, lv_pct(100));
	lv_obj_set_style_pad_hor(heading, 44, 0);
	lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT);
	lv_obj_center(heading);
	page.header_right = lv_obj_create(page.header);
	lv_obj_remove_style_all(page.header_right);
	lv_obj_set_size(page.header_right, 40, BEPIO_PAGE_HEADER_HEIGHT);
	lv_obj_align(page.header_right, LV_ALIGN_RIGHT_MID, 0, 0);
	page.container = lv_obj_create(page.screen);
	lv_obj_set_pos(page.container, 0, BEPIO_STATUS_BAR_HEIGHT + BEPIO_PAGE_HEADER_HEIGHT);
	lv_obj_set_size(page.container, lv_pct(100), lv_display_get_vertical_resolution(NULL) -
		BEPIO_STATUS_BAR_HEIGHT - BEPIO_PAGE_HEADER_HEIGHT - BEPIO_PAGE_BOTTOM_HEIGHT);
	lv_obj_set_style_border_width(page.container, 0, 0);
	lv_obj_set_style_radius(page.container, 0, 0);
	lv_obj_set_style_pad_all(page.container, 8, 0);
	lv_obj_set_style_bg_color(page.container, lv_color_hex(0xc7d5b8), 0);
	lv_obj_set_style_text_color(page.container, lv_color_hex(0x21392a), 0);
	lv_obj_set_scroll_dir(page.container, LV_DIR_VER);
	current_content = page.container;
	current_header_right = page.header_right;
	current_heading = heading;
	lv_obj_add_event_cb(page.container, content_deleted, LV_EVENT_DELETE, NULL);
	if (allow_back) {
		lv_obj_t *handle = lv_obj_create(page.screen);
		lv_obj_remove_style_all(handle);
		lv_obj_set_size(handle, 64, 4);
		lv_obj_set_style_bg_color(handle, lv_color_hex(0x52684a), 0);
		lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
		lv_obj_set_style_radius(handle, 2, 0);
		lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, -6);
	}
	lv_obj_update_layout(page.screen);
	return page;
}
