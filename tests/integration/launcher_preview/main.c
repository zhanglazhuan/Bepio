#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "bepio_launcher.h"
#include "bepio_app_manager.h"
#include "lv_page.h"
#include "launcher_gesture.h"
#include "lv_toast.h"
#include "apps/contacts/controller.h"
#include "system/uilv/imgs/icon_40x40/bepio_icons.h"

int bepio_messages_compose(const char *phone, const char *name)
{
	assert(phone && name); return 0;
}

contacts_controller_t *contacts_app_get_controller(void)
{
	static contacts_controller_t controller;
	return &controller;
}

void contacts_controller_set_compose_handler(contacts_controller_t *controller,
	int (*handler)(const char *phone, const char *name))
{
	controller->compose_message = handler;
}

static uint16_t frame[240 * 320], draw[320 * 16];
static bepio_application_t apps[] = {{.name="Contacts"}, {.name="Messages"}, {.name="Settings"}};
static bepio_app_manager_cb_fn closed;
static const char *current;
static bool consume_back;
static unsigned force_stops;
static lv_indev_data_t pointer_data;
int bepio_app_manager_get_num_apps(void) { return 3; }
bepio_application_t *bepio_app_manager_get_app(int i) { return i >= 0 && i < 3 ? &apps[i] : NULL; }
int bepio_app_manager_show(bepio_app_manager_cb_fn cb, void *root, void *group, const char *name)
{
	(void)group;
	closed = cb; current = name;
	lv_label_set_text(lv_label_create(root), name);
	return 0;
}
int bepio_app_manager_exit_app(void)
{
	if (consume_back) { consume_back = false; return 0; }
	current = NULL; if (closed) { closed(); } return 0;
}
int bepio_app_manager_delete(void) { force_stops++; current = NULL; return 0; }
bool bepio_app_manager_is_app_running(void) { return current != NULL; }
const char *bepio_app_manager_get_current_app_name(void) { return current; }
static void flush(lv_display_t *display, const lv_area_t *a, uint8_t *pixels)
{
	uint16_t *src = (uint16_t *)pixels;
	for (int y=a->y1; y<=a->y2; y++) {
		memcpy(frame+y*320+a->x1, src, (a->x2-a->x1+1)*2);
		src += a->x2-a->x1+1;
	}
	lv_display_flush_ready(display);
}
static void capture(const char *path)
{
	lv_refr_now(NULL);
	FILE *out=fopen(path,"wb"); assert(out);
	fprintf(out,"P6\n320 240\n255\n");
	for (int i=0;i<240*320;i++) {
		uint16_t p=frame[i];
		fputc(((p>>11)&31)*255/31,out);fputc(((p>>5)&63)*255/63,out);fputc((p&31)*255/31,out);
	}
	fclose(out);
}
static lv_obj_t *find(lv_obj_t *parent, const char *value)
{
	if (lv_obj_check_type(parent, &lv_label_class) && !strcmp(lv_label_get_text(parent), value)) { return parent; }
	for (uint32_t i=0;i<lv_obj_get_child_count(parent);i++) {
		lv_obj_t *found=find(lv_obj_get_child(parent,i),value);
		if (found) { return found; }
	}
	return NULL;
}
static lv_obj_t *find_icon(lv_obj_t *parent, const void *source)
{
	if (lv_obj_check_type(parent, &lv_image_class) && lv_image_get_src(parent) == source) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_icon(lv_obj_get_child(parent, i), source);
		if (found) { return found; }
	}
	return NULL;
}
static void click_icon(const void *source)
{
	lv_obj_t *icon = find_icon(lv_screen_active(), source); assert(icon);
	lv_obj_send_event(lv_obj_get_parent(icon), LV_EVENT_CLICKED, NULL);
	lv_tick_inc(20); lv_timer_handler();
}
static void click(lv_obj_t *parent, const char *value)
{
	lv_obj_t *label=find(parent,value); assert(label);
	lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,NULL);
	lv_tick_inc(20); lv_timer_handler();
}
static void pointer_read(lv_indev_t *input, lv_indev_data_t *data) { (void)input; *data=pointer_data; }
static void sample(lv_indev_t *input, int x, int y, bool pressed)
{
	pointer_data.point = (lv_point_t){x,y};
	pointer_data.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
	lv_tick_inc(20); lv_indev_read(input);
}
static void swipe(lv_indev_t *input, int x1, int y1, int x2, int y2)
{
	lv_obj_update_layout(lv_screen_active());
	sample(input,x1,y1,true); sample(input,x2,y2,true); sample(input,x2,y2,false);
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	lv_init();
	lv_display_t *display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_status_bar_init();
	lv_indev_t *input=lv_indev_create();
	lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(input,pointer_read);
	launcher_gesture_attach_input(input);
	launcher_gesture_attach_input(input); /* No duplicate hooks. */
	bepio_launcher_home();
	capture(argv[1]);
	lv_obj_t *contacts_icon = find_icon(lv_screen_active(), &ic_contacts);
	lv_obj_t *messages_icon = find_icon(lv_screen_active(), &ic_mail);
	lv_obj_t *settings_icon = find_icon(lv_screen_active(), &ic_settings);
	assert(contacts_icon && messages_icon && settings_icon);
	assert(lv_obj_get_x(lv_obj_get_parent(contacts_icon)) < lv_obj_get_x(lv_obj_get_parent(messages_icon)));
	assert(lv_obj_get_x(lv_obj_get_parent(messages_icon)) < lv_obj_get_x(lv_obj_get_parent(settings_icon)));
	assert(find(lv_layer_top(),"--:--") && find(lv_layer_top(),"NB --"));
	unsigned top_children=lv_obj_get_child_count(lv_layer_top());
	swipe(input,120,230,120,165); assert(!find(lv_layer_top(),"Exit app?"));
	click_icon(&ic_settings);
	assert(current && !strcmp(current,"Settings"));
	assert(lv_obj_get_child_count(lv_layer_top())==top_children);
	lv_obj_t *back=find(lv_screen_active(),LV_SYMBOL_LEFT); assert(back);
	lv_obj_update_layout(lv_screen_active());
	lv_area_t bounds; lv_obj_get_coords(lv_obj_get_parent(back),&bounds);
	assert(bounds.x1==0 && bounds.y1>=LV_STATUS_BAR_HEIGHT && bounds.y2<LV_STATUS_BAR_HEIGHT+LV_PAGE_HEADER_HEIGHT);
	consume_back=true;
	click(lv_screen_active(),LV_SYMBOL_LEFT); assert(current); /* App consumes nested back. */
	swipe(input,120,120,120,60); assert(!find(lv_layer_top(),"Exit app?"));
	swipe(input,120,230,120,210); assert(!find(lv_layer_top(),"Exit app?"));
	swipe(input,20,230,160,170); assert(!find(lv_layer_top(),"Exit app?"));
	swipe(input,120,230,120,165); assert(find(lv_layer_top(),"Exit app?"));
	char path[512]; snprintf(path,sizeof(path),"%s.exit.ppm",argv[1]); capture(path);
	click(lv_layer_top(),"Cancel"); assert(current && force_stops==0);
	assert(!find(lv_layer_top(),"Exit app?"));
	swipe(input,120,230,120,165); assert(find(lv_layer_top(),"Exit app?"));
	click(lv_layer_top(),"Exit"); assert(!current && force_stops==1);
	assert(find(lv_screen_active(),"Settings") && find(lv_screen_active(),"Contacts") &&
		find(lv_screen_active(),"Messages"));
	assert(!find(lv_screen_active(),"BEPIO") && !find(lv_screen_active(),"APPLICATIONS"));
	click_icon(&ic_mail); assert(current && !strcmp(current,"Messages"));
	snprintf(path,sizeof(path),"%s.app.ppm",argv[1]); capture(path);
	click(lv_screen_active(),LV_SYMBOL_LEFT); assert(!current);
	bepio_status_bar_update("12:34","Offline",85,true);
	assert(find(lv_layer_top(),"12:34") && find(lv_layer_top(),"Offline"));
	bepio_status_bar_update(NULL,NULL,-1,false);
	assert(find(lv_layer_top(),"--:--"));
	lv_toast_show("Saved", 10);
	assert(find(lv_layer_top(),"Saved"));
	lv_tick_inc(20); lv_timer_handler();
	assert(!find(lv_layer_top(),"Saved"));
	lv_toast_show("Dismiss", 1000);
	lv_obj_t *toast = find(lv_layer_top(),"Dismiss"); assert(toast);
	lv_obj_delete(toast);
	lv_tick_inc(1001); lv_timer_handler();
	lv_refr_now(display);
	puts("PASS: shared page/header, persistent status, nested back, real pointer bottom swipe thresholds, cancel/confirm exit, Home routing");
	return 0;
}
