#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "apps/settings/app.h"
#include "apps/settings/pages/settings_wifi.h"
#include "bepio_app_manager.h"
#include "lv_page.h"
#include "bepio_account.h"
#include "bepio_device_info.h"
#include "bepio_storage_info.h"
#include "bepio_ui_preferences.h"
#include <errno.h>

static uint16_t frame[240 * 320], draw[320 * 16];
static lv_display_t *display;
static lv_obj_t *root;
static void page_back(lv_event_t *event)
{
	(void)event;
	bepio_app_manager_exit_app();
}
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
static lv_obj_t *find_text(lv_obj_t *parent, const char *value)
{
	if (lv_obj_check_type(parent, &lv_label_class) && !strcmp(lv_label_get_text(parent), value)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_text(lv_obj_get_child(parent, i), value);
		if (found) { return found; }
	}
	return NULL;
}
static void click(const char *value)
{
	lv_obj_t *label = find_text(lv_screen_active(), value); assert(label);
	lv_obj_t *button = lv_obj_get_parent(label);
	while (button && !lv_obj_check_type(button, &lv_button_class)) { button = lv_obj_get_parent(button); }
	assert(button); lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);
	lv_refr_now(display);
}

static char applied_zone[64];
static bool applied_24, applied_save;
void bepio_ui_preferences_apply(const char *zone, bool h24, bool saving)
{
	snprintf(applied_zone, sizeof(applied_zone), "%s", zone);
	applied_24 = h24; applied_save = saving;
}
void bepio_account_snapshot(bepio_account_snapshot_t *a)
{
	*a = (bepio_account_snapshot_t){.confirmed = false};
	snprintf(a->id, sizeof(a->id), "bepio-aabbccddeeff");
}
int bepio_account_request(const char *id) { (void)id; return -ENETDOWN; }
void bepio_device_info(char *buffer, size_t size)
{
	snprintf(buffer, size, "Bepio v1\nESP32-S3\nHost preview fixture");
}
static bool files_mounted;
static unsigned retention_days = 7;
unsigned bepio_messaging_retention_days(void) { return retention_days; }
int bepio_messaging_set_retention_days(unsigned days) { retention_days = days; return 0; }
int bepio_storage_info(bepio_storage_info_t *info)
{
	*info = (bepio_storage_info_t){.flash_bytes = 16 * 1024 * 1024, .app_capacity = 3 * 1024 * 1024,
		.app_used = 1469712, .app_valid = true, .files_capacity = 0x9f0000,
		.files_valid = files_mounted, .files_total = 9 * 1024 * 1024, .files_used = 3 * 1024 * 1024,
		.files_error = files_mounted ? 0 : -ENODEV, .nvs_valid = true, .nvs_used_entries = 40, .nvs_total_entries = 756};
	return 0;
}
static bepio_wifi_snapshot_t wifi_fixture;
void bepio_wifi_snapshot(bepio_wifi_snapshot_t *out) { *out = wifi_fixture; }
int bepio_wifi_set_enabled(bool enabled)
{
	wifi_fixture.enabled = enabled; wifi_fixture.revision++;
	snprintf(wifi_fixture.status, sizeof(wifi_fixture.status), "%s", enabled ? "Select a network" : "Wi-Fi is off");
	if (enabled) {
		wifi_fixture.count = 2;
		wifi_fixture.aps[0] = (bepio_wifi_ap_t){.ssid = "Home Wi-Fi", .secured = true, .supported = true, .rssi = -45, .channel = 6};
		wifi_fixture.aps[1] = (bepio_wifi_ap_t){.ssid = "Guest Wi-Fi", .supported = true, .rssi = -64, .channel = 11};
	} else { wifi_fixture.count = 0; wifi_fixture.connected = false; }
	return 0;
}
int bepio_wifi_scan(void) { wifi_fixture.revision++; return 0; }
int bepio_wifi_connect(const bepio_wifi_ap_t *ap, const char *password)
{
	if (ap->secured && password && strlen(password) < 8) { return -EINVAL; }
	wifi_fixture.connected = true; wifi_fixture.revision++;
	snprintf(wifi_fixture.ssid, sizeof(wifi_fixture.ssid), "%s", ap->ssid);
	snprintf(wifi_fixture.ip, sizeof(wifi_fixture.ip), "192.0.2.10");
	return 0;
}
int bepio_wifi_disconnect(void) { wifi_fixture.connected = false; wifi_fixture.revision++; return 0; }
static lv_obj_t *find_type(lv_obj_t *parent, const lv_obj_class_t *type)
{
	if (lv_obj_check_type(parent, type)) { return parent; }
	for (uint32_t i = 0; i < lv_obj_get_child_count(parent); i++) {
		lv_obj_t *found = find_type(lv_obj_get_child(parent, i), type);
		if (found) { return found; }
	}
	return NULL;
}
static void back_home(settings_controller_t *c)
{
	assert(settings_controller_back(c));
	assert(c->view->page == SETTINGS_PAGE_HOME);
}
int main(int argc, char **argv)
{
	assert(argc == 2); out_dir = argv[1];
	lv_init(); display = lv_display_create(320, 240);
	lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display, draw, NULL, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display, flush);
	bepio_page_t page = bepio_page_create("Settings", true, page_back, NULL);
	lv_screen_load(page.screen); root = page.container;
	assert(bepio_settings_register() == 0);
	assert(bepio_app_manager_show(NULL, root, NULL, "Settings") == 0);
	settings_controller_t *c = settings_app_get_controller();
	screenshot("settings-home");
	lv_obj_t *card = lv_obj_get_parent(find_text(root, "bepio-aabbccddeeff"));
	lv_obj_t *row = lv_obj_get_parent(find_text(root, "Datetime"));
	assert(lv_obj_get_y(lv_obj_get_parent(find_text(root, "Usage"))) <
		lv_obj_get_y(lv_obj_get_parent(find_text(root, "About"))));
	assert(lv_obj_get_height(card) == 44 && lv_obj_get_height(row) == 33);
	assert(!find_text(root, "ID"));
	lv_obj_scroll_to_view(lv_obj_get_parent(find_text(root, "About")), LV_ANIM_OFF);
	lv_refr_now(display);
	lv_area_t bounds; lv_obj_get_coords(find_text(root, "About"), &bounds);
	assert(bounds.y2 < 240 - BEPIO_PAGE_BOTTOM_HEIGHT);
	assert(lv_obj_get_y(lv_obj_get_parent(find_text(root, "Wi-Fi"))) < lv_obj_get_y(row));
	click("Wi-Fi"); assert(c->view->page == SETTINGS_PAGE_WIFI);
	lv_obj_t *wifi_toggle = find_type(root, &lv_switch_class); assert(wifi_toggle);
	assert(!find_text(root, "Wi-Fi is off"));
	assert(!find_text(root, "Enable Wi-Fi to find networks"));
	lv_obj_add_state(wifi_toggle, LV_STATE_CHECKED);
	lv_obj_send_event(wifi_toggle, LV_EVENT_VALUE_CHANGED, NULL);
	assert(find_text(root, "Home Wi-Fi"));
	wifi_fixture.scanning = true; wifi_fixture.revision++;
	snprintf(wifi_fixture.status, sizeof(wifi_fixture.status), "Scanning...");
	bepio_settings_wifi_refresh();
	lv_obj_t *scanning = find_text(root, "Scanning..."); assert(scanning);
	lv_obj_update_layout(root);
	assert(lv_obj_get_parent(scanning) == root);
	assert(lv_obj_get_y(scanning) == 38);
	assert(lv_obj_get_style_text_align(scanning, 0) == LV_TEXT_ALIGN_CENTER);
	wifi_fixture.scanning = false; wifi_fixture.revision++;
	bepio_settings_wifi_refresh();
	assert(!find_text(root, "-45dBm / Secured"));
	assert(!find_text(root, "-64dBm / Open"));
	lv_obj_update_layout(root);
	assert(lv_obj_get_height(lv_obj_get_parent(find_text(root, "Home Wi-Fi"))) == 33);
	assert(!find_text(root, "Scan"));
	assert(wifi_fixture.enabled);
	lv_tick_inc(300); lv_timer_handler(); screenshot("settings-wifi");
	click("Home Wi-Fi"); assert(c->view->page == SETTINGS_PAGE_WIFI_CONNECT);
	assert(!find_text(root, "Enter password to connect"));
	lv_obj_t *password = find_type(root, &lv_textarea_class); assert(password);
	assert(lv_textarea_get_password_mode(password));
	lv_obj_t *show_icon = find_type(root, &lv_image_class); assert(show_icon);
	lv_obj_t *show = lv_obj_get_parent(show_icon); assert(show);
	lv_obj_send_event(show, LV_EVENT_CLICKED, NULL);
	assert(!lv_textarea_get_password_mode(password));
	lv_textarea_set_text(password, "short"); click("Connect");
	assert(find_text(root, "8-63 chars / 64 hex digits"));
	lv_tick_inc(300); lv_timer_handler(); screenshot("settings-wifi-password");
	lv_textarea_set_text(password, "testpassword"); click("Connect");
	assert(find_text(root, "Connected: 192.0.2.10"));
	screenshot("settings-wifi-connected");
	assert(settings_controller_back(c)); assert(c->view->page == SETTINGS_PAGE_WIFI);
	back_home(c);
	click("Datetime"); assert(c->view->page == SETTINGS_PAGE_DATETIME);
	click("12h"); assert(!c->model->hour_24 && !applied_24);
	lv_obj_t *dropdown = find_type(root, &lv_dropdown_class); assert(dropdown);
	lv_dropdown_set_selected(dropdown, 6);
	lv_obj_send_event(dropdown, LV_EVENT_VALUE_CHANGED, NULL);
	assert(c->model->timezone == 6 && !strcmp(applied_zone, bepio_settings_timezones[6].posix));
	screenshot("settings-datetime"); back_home(c);
	click("Battery");
	lv_obj_t *toggle = find_type(root, &lv_switch_class); assert(toggle);
	lv_obj_add_state(toggle, LV_STATE_CHECKED);
	lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, NULL);
	assert(c->model->power_save && applied_save);
	screenshot("settings-battery"); back_home(c);
	click("Updates"); assert(c->view->page == SETTINGS_PAGE_UPDATES);
	lv_obj_t *check = lv_obj_get_parent(find_text(root, "Check for updates"));
	assert(check && !lv_obj_has_state(check, LV_STATE_DISABLED));
	screenshot("settings-updates-ready");
	click("Check for updates");
	assert(find_text(root, "Checking mock HTTPS manifest..."));
	lv_tick_inc(650); lv_timer_handler();
	assert(find_text(root, "Mock update available: v1.1.0"));
	click("Simulate update");
	for (unsigned i = 0; i < 5; i++) { lv_tick_inc(300); lv_timer_handler(); }
	assert(find_text(root, "Demo complete; firmware unchanged"));
	screenshot("settings-updates"); back_home(c);
	click("Updates"); click("Check for updates"); back_home(c);
	lv_tick_inc(1000); lv_timer_handler();
	assert(c->view->page == SETTINGS_PAGE_HOME);
	click("Usage"); assert(c->view->page == SETTINGS_PAGE_USAGE);
	assert(find_text(root, "TX: 0 bytes\nRX: 0 bytes\nConnected: 0 min 0 sec\nChange cooldown: 0 sec"));
	lv_tick_inc(1000); lv_timer_handler(); screenshot("settings-usage"); back_home(c);
	lv_obj_update_layout(root);
	assert(lv_obj_get_y(lv_obj_get_parent(find_text(root, "Storage"))) < lv_obj_get_y(lv_obj_get_parent(find_text(root, "Usage"))));
	click("Storage"); assert(c->view->page == SETTINGS_PAGE_STORAGE);
	assert(find_text(root, "Internal Flash: 16.00 MiB"));
	assert(find_text(root, "Not mounted; usage unknown"));
	assert(find_text(root, "Delete chats older than"));
	assert(retention_days == 7);
	click("3 days"); assert(retention_days == 3);
	click("14 days"); assert(retention_days == 14);
	screenshot("settings-storage"); back_home(c);
	files_mounted = true; click("Storage");
	assert(find_text(root, "Files: 3.00 MiB / 9.00 MiB"));
	assert(find_text(root, "Available files: 6.00 MiB"));
	lv_obj_scroll_to_y(root, 190, LV_ANIM_OFF);
	screenshot("settings-storage-retention");
	screenshot("settings-storage-mounted"); back_home(c);
	click("About"); assert(find_text(root, "Bepio v1\nESP32-S3\nHost preview fixture"));
	screenshot("settings-about"); back_home(c);
	click("bepio-aabbccddeeff"); assert(c->view->page == SETTINGS_PAGE_ID);
	lv_obj_t *entry = find_type(root, &lv_textarea_class); assert(entry);
	lv_obj_update_layout(root);
	lv_obj_t *id_summary = lv_obj_get_child(root, 0);
	assert(lv_obj_get_y(entry) - (lv_obj_get_y(id_summary) + lv_obj_get_height(id_summary)) == 4);
	lv_textarea_set_text(entry, "my_id"); click("Confirm ID change");
	assert(find_text(root, "Offline: backend connection required"));
	screenshot("settings-id"); back_home(c);
	assert(bepio_app_manager_delete() == 0);
	assert(bepio_app_manager_show(NULL, root, NULL, "Settings") == 0);
	assert(!c->model->hour_24 && c->model->timezone == 6 && c->model->power_save);
	assert(bepio_app_manager_delete() == 0);
	puts("PASS Settings card/list geometry, pages/back, timezone/format, power saving, OTA state, ID and reopen");
}
