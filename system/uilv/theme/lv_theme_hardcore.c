#include "lv_theme_hardcore.h"

static lv_theme_t *lcd_theme;

lv_theme_t *lv_theme_hardcore_init(lv_display_t *disp, bool dark_bg,
	const lv_font_t *font)
{
	if (!disp) { return NULL; }
	lcd_theme = lv_theme_default_init(disp, lv_color_hex(0x24785e),
		lv_color_hex(0x346bd3), dark_bg, font ? font : LV_FONT_DEFAULT);
	return lcd_theme;
}

bool lv_theme_hardcore_is_inited(void)
{
	return lcd_theme != NULL;
}

lv_theme_t *lv_theme_hardcore_get(void)
{
	return lcd_theme;
}

void lv_theme_hardcore_deinit(void)
{
	/* LVGL owns its default theme for the lifetime of the display. */
	lcd_theme = NULL;
}
