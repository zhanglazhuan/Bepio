#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_floating_button.h"
#include "system/uilv/imgs/icon_28x28/bepio_icons_28.h"
#include "lv_page.h"
#include <errno.h>

static uint16_t frame[240 * 320], draw[320 * 16];
static lv_display_t *display;

static const char *out_dir;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *pixels)
{
	uint16_t *src = (uint16_t *)pixels;
	for (int y = a->y1; y <= a->y2; y++) {
		memcpy(frame + y * 320 + a->x1, src, (a->x2 - a->x1 + 1) * 2);
		src += a->x2 - a->x1 + 1;
	}
	lv_display_flush_ready(d);
}
static void screenshot(const char *name)
{
	lv_refr_now(display);
	char path[512]; snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
	FILE *f = fopen(path, "wb"); assert(f);
	fprintf(f, "P6\n320 240\n255\n");
	for (int i = 0; i < 240 * 320; i++) {
		uint16_t p = frame[i];
		fputc(((p >> 11) & 31) * 255 / 31, f);
		fputc(((p >> 5) & 63) * 255 / 63, f);
		fputc((p & 31) * 255 / 31, f);
	}
	fclose(f);
}

static lv_indev_data_t pointer;
static unsigned clicks;
static int callback_data;
static void input_read(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data = pointer; }
static void clicked(lv_event_t *event)
{
	assert(lv_event_get_user_data(event) == &callback_data); clicks++;
}
static void tap(lv_indev_t *input, int x, int y)
{
	pointer.point = (lv_point_t){x, y};
	pointer.state = LV_INDEV_STATE_PRESSED; lv_tick_inc(30); lv_indev_read(input);
	pointer.state = LV_INDEV_STATE_RELEASED; lv_tick_inc(30); lv_indev_read(input);
}
int main(int argc, char **argv)
{
	assert(argc == 2); out_dir = argv[1];
	lv_init(); display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_page_t page = bepio_page_create("Floating Button", true, NULL, NULL);
	lv_screen_load(page.screen);
	lv_obj_t *root = page.container;
	lv_obj_set_style_pad_all(root, 0, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	for (unsigned i = 0; i < 10; i++) {
		lv_obj_t *row = lv_obj_create(root); lv_obj_set_size(row, 230, 36);
		lv_obj_set_style_pad_all(row, 4, 0);
		lv_obj_t *text = lv_label_create(row); lv_label_set_text_fmt(text, "Scrollable item %u", i + 1);
	}
	lv_obj_t *button = bepio_floating_button_create(root, NULL, clicked, &callback_data); assert(button);
	lv_obj_update_layout(page.screen);
	assert(lv_obj_get_width(button) == 48 && lv_obj_get_height(button) == 48);
	assert(lv_obj_has_flag(button, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT));
	assert(!lv_obj_has_flag(button, LV_OBJ_FLAG_SCROLLABLE));
	assert(!strcmp(lv_label_get_text(lv_obj_get_child(button, 0)), LV_SYMBOL_PLUS));
	lv_area_t before, after; lv_obj_get_coords(button, &before);
	assert(before.x2 == 320 - BEPIO_FLOATING_BUTTON_MARGIN - 1);
	lv_obj_scroll_to_y(root, 100, LV_ANIM_OFF); lv_obj_update_layout(root);
	lv_obj_get_coords(button, &after); assert(!memcmp(&before, &after, sizeof(before)));
	lv_indev_t *input = lv_indev_create(); lv_indev_set_type(input, LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(input, input_read);
	tap(input, (before.x1 + before.x2) / 2, (before.y1 + before.y2) / 2); assert(clicks == 1);
	tap(input, before.x1 - 5, (before.y1 + before.y2) / 2); assert(clicks == 2);
	lv_obj_add_state(button, LV_STATE_DISABLED);
	tap(input, (before.x1 + before.x2) / 2, (before.y1 + before.y2) / 2); assert(clicks == 2);
	lv_obj_remove_state(button, LV_STATE_DISABLED);
	lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
	tap(input, (before.x1 + before.x2) / 2, (before.y1 + before.y2) / 2); assert(clicks == 2);
	lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
	screenshot("floating-button-symbol");
	lv_obj_delete(button);
	button = bepio_floating_button_create_image(root, &ic_send_28, clicked, &callback_data); assert(button);
	assert(lv_image_get_src(lv_obj_get_child(button, 0)) == &ic_send_28);
	screenshot("floating-button-image");
	lv_obj_delete(button);
	button = bepio_floating_button_create(root, LV_SYMBOL_HOME, NULL, NULL); assert(button);
	assert(!bepio_floating_button_create(NULL, NULL, NULL, NULL));
	assert(!bepio_floating_button_create_image(root, NULL, NULL, NULL));
	lv_obj_delete(page.screen);
	puts("PASS floating button symbol/image, anchoring, flex/scroll independence, pointer hit padding, disabled/hidden and parent cleanup");
}
