#include "lv_controls.h"

lv_obj_t *bepio_ui_label(lv_obj_t *parent, const char *value, int x, int y, int width)
{
	lv_obj_t *label = lv_label_create(parent);
	lv_label_set_text(label, value);
	lv_obj_set_pos(label, x, y); lv_obj_set_width(label, width);
	lv_obj_set_style_text_color(label, lv_color_hex(0x21392a), 0);
	return label;
}

lv_obj_t *bepio_ui_button(lv_obj_t *parent, const char *value, int x, int y,
	int width, int height, lv_event_cb_t cb, void *data)
{
	lv_obj_t *button = lv_button_create(parent);
	lv_obj_set_pos(button, x, y); lv_obj_set_size(button, width, height);
	lv_obj_set_style_radius(button, 6, 0);
	lv_obj_set_style_bg_color(button, lv_color_hex(0x405d43), 0);
	lv_obj_set_style_shadow_width(button, 0, 0);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_t *label = lv_label_create(button);
	lv_label_set_text(label, value); lv_obj_center(label);
	if (cb) { lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, data); }
	return button;
}
