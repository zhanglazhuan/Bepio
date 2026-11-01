#include "ui_fonts.h"

lv_font_t custom_font_normal;

void ui_fonts_init(void)
{
	/* No external glyph tables or Flash-to-RAM fallback chain on this board. */
	custom_font_normal = *LV_FONT_DEFAULT;
}
