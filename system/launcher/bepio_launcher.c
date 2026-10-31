/* App grid and app-manager routing inspired by NomadCast/EPOS launcher.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <string.h>
#include <stdio.h>
#include "apps/contacts/model.h"
#include "bepio_launcher.h"
#include "bepio_app_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_page.h"
#include "bepio_navigation.h"
#include "apps/messages/navigation.h"
#include "system/uilv/imgs/icon_40x40/bepio_icons.h"

static const char *TAG = "launcher";
static lv_obj_t *screen;

static lv_obj_t *label(lv_obj_t *parent, const char *text, uint32_t color)
{
	lv_obj_t *obj = lv_label_create(parent);
	lv_label_set_text(obj, text);
	lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
	return obj;
}

static void load_screen(lv_obj_t *next)
{
	lv_obj_t *old = screen;
	screen = next;
	lv_screen_load(screen);
	if (old) { lv_obj_delete_async(old); }
}

static void force_home(void)
{
	int ret = bepio_app_manager_delete();
	if (ret) { ESP_LOGW(TAG, "Cannot exit app: %d", ret); return; }
	bepio_launcher_home();
}

static void back(lv_event_t *event)
{
	(void)event;
	int ret = bepio_app_manager_exit_app();
	if (ret) { ESP_LOGW(TAG, "App back: %d", ret); }
}

static int open_named_app(const char *name)
{
	bepio_page_t page = bepio_page_create(name, true, back, NULL);
	load_screen(page.screen);
	bepio_navigation_set_exit_handler(force_home, name);
	int ret = bepio_app_manager_show(bepio_launcher_home, page.container, NULL, name);
	if (ret) { bepio_launcher_home(); }
	return ret;
}
static int compose_message(const char *phone, const char *name)
{
	/* App shutdown invalidates the caller's view; retain recipient locally. */
	char recipient[CONTACTS_PHONE_MAX + 1], display_name[CONTACTS_NAME_MAX + 1];
	snprintf(recipient, sizeof(recipient), "%s", phone);
	snprintf(display_name, sizeof(display_name), "%s", name);
	int ret = bepio_app_manager_delete();
	if (!ret) { ret = open_named_app("Messages"); }
	if (!ret) { ret = bepio_messages_compose(recipient, display_name); }
	return ret;
}
static void open_app(lv_event_t *event)
{
	bepio_application_t *app = lv_event_get_user_data(event);
	if (app && app->name) { open_named_app(app->name); }
}

void bepio_launcher_home(void)
{
	bepio_page_set_message_handler(compose_message);
	bepio_navigation_set_exit_handler(NULL, NULL);
	bepio_page_t page = bepio_page_create(NULL, false, NULL, NULL);
	lv_obj_add_flag(page.header, LV_OBJ_FLAG_HIDDEN);
	lv_obj_set_y(page.container, BEPIO_STATUS_BAR_HEIGHT);
	lv_obj_set_height(page.container, lv_display_get_vertical_resolution(NULL) -
		BEPIO_STATUS_BAR_HEIGHT - BEPIO_PAGE_BOTTOM_HEIGHT);
	load_screen(page.screen);
	lv_obj_set_style_pad_all(page.container, 0, 0);
	int count = bepio_app_manager_get_num_apps(), visible = 0;
	for (int i = 0; i < count; i++) {
		bepio_application_t *app = bepio_app_manager_get_app(i);
		if (!app || app->hidden) { continue; }
		uint32_t color = 0x405d43;
		const lv_image_dsc_t *icon = NULL;
		if (!strcmp(app->name, "Settings")) { icon = &ic_settings; }
		if (!strcmp(app->name, "Contacts")) { icon = &ic_contacts; }
		if (!strcmp(app->name, "Messages")) { icon = &ic_mail; }
		if (!strcmp(app->name, "Chat")) { icon = &ic_voice_chat; }
		lv_obj_t *button = lv_button_create(page.container);
		lv_obj_set_size(button, 84, 62);
		lv_obj_set_pos(button, 12 + (visible % 3) * 102, 12 + (visible / 3) * 96);
		lv_obj_set_style_radius(button, 6, 0);
		lv_obj_set_style_shadow_width(button, 0, 0);
		lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
		lv_obj_add_event_cb(button, open_app, LV_EVENT_CLICKED, app);
		lv_obj_t *glyph;
		if (icon) {
			glyph = lv_image_create(button);
			lv_image_set_src(glyph, icon);
			lv_obj_set_style_image_recolor(glyph, lv_color_hex(0xe3ebd4), 0);
			lv_obj_set_style_image_recolor_opa(glyph, LV_OPA_COVER, 0);
		} else {
			glyph = label(button, LV_SYMBOL_SETTINGS, 0xe3ebd4);
			lv_obj_set_style_transform_scale_x(glyph, 384, 0);
			lv_obj_set_style_transform_scale_y(glyph, 384, 0);
		}
		lv_obj_center(glyph);
		lv_obj_t *name = label(page.container, app->name, 0x21392a);
		lv_obj_set_width(name, 96);
		lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_set_pos(name, 6 + (visible % 3) * 102, 82 + (visible / 3) * 96);
		visible++;
	}
	if (!visible) { lv_obj_center(label(page.container, "No applications", 0xb7c6dc)); }
	ESP_LOGI(TAG, "Home ready: %d visible applications", visible);
}
