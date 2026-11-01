#include "lv_ui_style_guide.h"

typedef struct {
	lv_style_t primary, secondary, dropdown;
	bool ready;
} lcd_styles_t;
static lcd_styles_t styles;

static void init_styles(void)
{
	if (styles.ready) { return; }
	lv_style_init(&styles.primary);
	lv_style_set_bg_opa(&styles.primary, LV_OPA_COVER);
	lv_style_set_bg_color(&styles.primary, lv_color_hex(0x24785e));
	lv_style_set_text_color(&styles.primary, lv_color_white());
	lv_style_set_border_width(&styles.primary, 0);
	lv_style_set_radius(&styles.primary, 8);
	lv_style_set_pad_all(&styles.primary, 8);
	lv_style_init(&styles.secondary);
	lv_style_set_bg_opa(&styles.secondary, LV_OPA_COVER);
	lv_style_set_bg_color(&styles.secondary, lv_color_hex(0xe6efea));
	lv_style_set_text_color(&styles.secondary, lv_color_hex(0x18352d));
	lv_style_set_border_width(&styles.secondary, 1);
	lv_style_set_border_color(&styles.secondary, lv_color_hex(0x91b7a7));
	lv_style_set_radius(&styles.secondary, 8);
	lv_style_set_pad_all(&styles.secondary, 8);
	lv_style_init(&styles.dropdown);
	lv_style_set_bg_opa(&styles.dropdown, LV_OPA_COVER);
	lv_style_set_bg_color(&styles.dropdown, lv_color_white());
	lv_style_set_text_color(&styles.dropdown, lv_color_hex(0x1b2938));
	lv_style_set_border_width(&styles.dropdown, 1);
	lv_style_set_border_color(&styles.dropdown, lv_color_hex(0xaabbb1));
	lv_style_set_radius(&styles.dropdown, 6);
	lv_style_set_pad_all(&styles.dropdown, 6);
	styles.ready = true;
}

void ui_style_set_btn_primary(lv_obj_t *btn)
{
	init_styles();
	lv_obj_add_style(btn, &styles.primary, 0);
}

void ui_style_set_btn_secondary(lv_obj_t *btn)
{
	init_styles();
	lv_obj_add_style(btn, &styles.secondary, 0);
}

void ui_style_set_dropdown(lv_obj_t *dd)
{
	init_styles();
	lv_obj_add_style(dd, &styles.dropdown, 0);
}

void ui_style_set_switch(lv_obj_t *sw)
{
	lv_obj_set_size(sw, UI_SWITCH_W, UI_SWITCH_H);
	lv_obj_set_style_bg_color(sw, lv_color_hex(0xc3cec9), LV_PART_INDICATOR);
	lv_obj_set_style_bg_color(sw, lv_color_hex(0x24785e),
		LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB);
	lv_obj_set_style_border_width(sw, 0, LV_PART_KNOB);
	lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
	lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
}

void ui_style_set_btn_size_large(lv_obj_t *btn) { lv_obj_set_height(btn, UI_BTN_H_LARGE); }
void ui_style_set_btn_size_medium(lv_obj_t *btn) { lv_obj_set_height(btn, UI_BTN_H_MEDIUM); }
void ui_style_set_btn_size_small(lv_obj_t *btn) { lv_obj_set_height(btn, UI_BTN_H_SMALL); }
