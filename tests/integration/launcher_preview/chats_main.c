#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_status_bar.h"
#include "apps/chats/controller.h"
#include "apps/chats/view.h"
#include "apps/contacts/app.h"
#include "system/uilv/imgs/icon_24x24/bepio_icons_24.h"

static uint16_t frame[320 * 240], draw[320 * 16];
static lv_display_t *display;
static bepio_message_t queued;
static contacts_controller_t contacts_controller;

contacts_controller_t *contacts_app_get_controller(void) { return &contacts_controller; }
int contacts_controller_lookup_at(contacts_controller_t *controller, size_t index, contacts_entry_t *entry)
{
	assert(controller == &contacts_controller);
	if (index) { return -ENOENT; }
	*entry = (contacts_entry_t){.id = 1, .name = "Ada", .phone = "13800138000"};
	return 0;
}
int bepio_messaging_get_at(bepio_message_box_t box, size_t index, bepio_message_t *message)
{
	if (box == BEPIO_MESSAGE_OUTBOX && !index && queued.id) {
		*message = queued;
		return 0;
	}
	return -ENOENT;
}
int bepio_messaging_get(bepio_message_box_t box, uint32_t id, bool mark_read, bepio_message_t *message)
{
	assert(!mark_read);
	if (box == BEPIO_MESSAGE_OUTBOX && id == queued.id && queued.id) {
		*message = queued;
		return 0;
	}
	return -ENOENT;
}
int bepio_messaging_submit(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id)
{
	assert(!draft_id && !strcmp(payload->phone, "13800138000"));
	assert(!strcmp(payload->body, "你好"));
	queued = (bepio_message_t){.id = 1, .state = BEPIO_MESSAGE_QUEUED, .payload = *payload};
	*id = queued.id;
	return 0;
}
int bepio_messages_voice_play(const char *text) { (void)text; return 0; }

static int start(void *context) { (void)context; return 0; }
static int stop(void *context, char *text, size_t capacity)
{
	(void)context;
	snprintf(text, capacity, "%s", "你好");
	return 0;
}
static void cancel(void *context) { (void)context; }
static const bepio_chats_voice_input_t input = {
	.start = start, .stop = stop, .cancel = cancel,
};

static void flush(lv_display_t *d, const lv_area_t *area, uint8_t *pixels)
{
	uint16_t *src = (uint16_t *)pixels;
	for (int y = area->y1; y <= area->y2; y++) {
		memcpy(frame + y * 320 + area->x1, src, (area->x2 - area->x1 + 1) * 2);
		src += area->x2 - area->x1 + 1;
	}
	lv_display_flush_ready(d);
}
static void screenshot(const char *directory, const char *name)
{
	lv_refr_now(display);
	char path[512];
	snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
	FILE *out = fopen(path, "wb");
	assert(out);
	fprintf(out, "P6\n320 240\n255\n");
	for (size_t i = 0; i < 320 * 240; i++) {
		uint16_t pixel = frame[i];
		fputc(((pixel >> 11) & 31) * 255 / 31, out);
		fputc(((pixel >> 5) & 63) * 255 / 63, out);
		fputc((pixel & 31) * 255 / 31, out);
	}
	fclose(out);
}
static lv_obj_t *find_label(lv_obj_t *parent, const char *value)
{
	if (lv_obj_check_type(parent, &lv_label_class) &&
	    !strcmp(lv_label_get_text(parent), value)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_label(lv_obj_get_child(parent, i), value);
		if (found) { return found; }
	}
	return NULL;
}
static lv_obj_t *find_mic(lv_obj_t *parent)
{
	if (lv_obj_check_type(parent, &lv_image_class) &&
	    lv_image_get_src(parent) == &ic_mic_24) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_mic(lv_obj_get_child(parent, i));
		if (found) { return found; }
	}
	return NULL;
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	lv_init();
	display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_status_bar_init();
	static bepio_chats_model_t model;
	bepio_chats_controller_t controller;
	bepio_chats_controller_open(&controller, &model);
	assert(!bepio_chats_view_open(lv_screen_active(), NULL, &controller));
	lv_timer_handler();
	lv_obj_t *screen = lv_screen_active();
	assert(find_label(screen, "No messages yet"));
	lv_obj_t *mic = find_mic(screen);
	assert(mic && lv_image_get_src(mic) == &ic_mic_24);
	lv_obj_t *button = find_label(screen, "Hold to talk");
	assert(button);
	button = lv_obj_get_parent(button);
	screenshot(argv[1], "chats-empty");
	lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
	assert(find_label(screen, "Speech input unavailable"));
	assert(!queued.id);
	assert(!bepio_chats_controller_set_voice_input(&controller, &input, NULL));
	lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
	assert(find_label(screen, "Recording... release to queue"));
	assert(!queued.id);
	lv_obj_send_event(button, LV_EVENT_PRESS_LOST, NULL);
	assert(find_label(screen, "Recording cancelled"));
	assert(!queued.id);
	lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
	lv_obj_send_event(button, LV_EVENT_RELEASED, NULL);
	assert(queued.id == 1);
	assert(find_label(screen, "Queued for sending"));
	assert(!find_label(screen, "No messages yet"));
	screenshot(argv[1], "chats-queued");
	bepio_chats_view_close();
	bepio_chats_controller_close(&controller);
	lv_obj_delete(screen);
	lv_timer_handler();
	puts("PASS: Chats empty conversation microphone, press, unavailable, release, queue");
	return 0;
}
