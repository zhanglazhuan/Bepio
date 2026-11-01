#ifndef LV_THEME_HARDCORE_H
#define LV_THEME_HARDCORE_H
#include "lvgl.h"

/* DeepStoa API name retained; implementation selects an LCD color theme. */
#define LV_FONT_TINY   &lv_font_montserrat_14
#define LV_FONT_SMALL  &lv_font_montserrat_14
#define LV_FONT_NORMAL &lv_font_montserrat_14
#define LV_FONT_LARGE  &lv_font_montserrat_20
#define EPOS_BOTTOM_BUTTON_HEIGHT 44

lv_theme_t *lv_theme_hardcore_init(lv_display_t *disp, bool dark_bg,
	const lv_font_t *font);
bool lv_theme_hardcore_is_inited(void);
lv_theme_t *lv_theme_hardcore_get(void);
void lv_theme_hardcore_deinit(void);
#endif
