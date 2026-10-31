/* Fixed circular action button adapted from DeepStoa.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "lv_floating_button.h"
static lv_obj_t *create_button(lv_obj_t *parent, lv_event_cb_t callback, void *user_data)
{
	if (!parent) { return NULL; }
	lv_obj_t *button = lv_button_create(parent);
	if (!button) { return NULL; }
	lv_obj_set_size(button, BEPIO_FLOATING_BUTTON_SIZE, BEPIO_FLOATING_BUTTON_SIZE);
	lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(button, lv_color_hex(0x405d43), 0);
	lv_obj_set_style_border_width(button, 0, 0);
	lv_obj_set_style_shadow_width(button, 0, 0);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_set_ext_click_area(button, BEPIO_FLOATING_BUTTON_TOUCH_PADDING);
	lv_obj_add_flag(button, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
	lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -BEPIO_FLOATING_BUTTON_MARGIN, -BEPIO_FLOATING_BUTTON_MARGIN);
	if (callback) { lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data); }
	return button;
}
lv_obj_t *bepio_floating_button_create(lv_obj_t *parent, const char *symbol,
	lv_event_cb_t callback, void *user_data)
{
	lv_obj_t *button = create_button(parent, callback, user_data);
	if (!button) { return NULL; }
	lv_obj_t *icon = lv_label_create(button);
	if (!icon) { lv_obj_delete(button); return NULL; }
	lv_label_set_text(icon, symbol ? symbol : LV_SYMBOL_PLUS);
	lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(icon, lv_color_hex(0xe3ebd4), 0);
	lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_center(icon);
	return button;
}
lv_obj_t *bepio_floating_button_create_image(lv_obj_t *parent, const lv_image_dsc_t *image,
	lv_event_cb_t callback, void *user_data)
{
	if (!image) { return NULL; }
	lv_obj_t *button = create_button(parent, callback, user_data);
	if (!button) { return NULL; }
	lv_obj_t *icon = lv_image_create(button);
	if (!icon) { lv_obj_delete(button); return NULL; }
	lv_image_set_src(icon, image);
	lv_obj_set_style_image_recolor(icon, lv_color_hex(0xe3ebd4), 0);
	lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
	lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_center(icon);
	return button;
}
