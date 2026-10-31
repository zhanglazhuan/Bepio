#include "settings_updates.h"
#include "settings_page_common.h"
/* Reserved example.com endpoint. This page never starts network/flash work. */
static const char *mock_url = "https://updates.example.com/bepio/manifest.json";
static lv_obj_t *status_label, *action_button, *action_label, *progress_bar;
static unsigned progress;
static bool available;
static void finish_timer(lv_timer_t *timer)
{
	bepio_settings_ui.timer = NULL;
	lv_timer_delete(timer);
}
static void checked(lv_timer_t *timer)
{
	finish_timer(timer);
	available = true;
	lv_label_set_text(status_label, "Mock update available: v1.1.0");
	lv_label_set_text(action_label, "Simulate update"); lv_obj_center(action_label);
	lv_obj_remove_state(action_button, LV_STATE_DISABLED);
}
static void downloading(lv_timer_t *timer)
{
	progress += 20;
	lv_bar_set_value(progress_bar, progress, LV_ANIM_OFF);
	lv_label_set_text_fmt(status_label, "Simulating update... %u%%", progress);
	if (progress >= 100) {
		finish_timer(timer);
		lv_label_set_text(status_label, "Demo complete; firmware unchanged");
		lv_label_set_text(action_label, "Demo complete"); lv_obj_center(action_label);
	}
}
static void run(lv_event_t *event)
{
	(void)event;
	if (bepio_settings_ui.timer) { return; }
	lv_obj_add_state(action_button, LV_STATE_DISABLED);
	if (!available) {
		lv_label_set_text(status_label, "Checking mock HTTPS manifest...");
		bepio_settings_ui.timer = lv_timer_create(checked, 600, NULL);
	} else {
		progress = 0;
		lv_label_set_text(status_label, "Simulating update... 0%");
		bepio_settings_ui.timer = lv_timer_create(downloading, 250, NULL);
	}
	if (!bepio_settings_ui.timer) {
		lv_label_set_text(status_label, "Cannot start demo; try again");
		lv_obj_remove_state(action_button, LV_STATE_DISABLED);
	}
}
void bepio_settings_updates_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	available = false; progress = 0;
	bepio_settings_label(root, "Remote OTA / MOCK", 12, 4, 296);
	bepio_settings_label(root, mock_url, 12, 27, 296);
	status_label = bepio_settings_label(root, "Demo only; no download or restart", 12, 76, 296);
	progress_bar = lv_bar_create(root);
	lv_obj_set_pos(progress_bar, 12, 105); lv_obj_set_size(progress_bar, 296, 8);
	lv_bar_set_range(progress_bar, 0, 100); lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
	action_button = bepio_settings_button(root, "Check for updates", 12, 130, 296, 30, run, NULL);
	action_label = lv_obj_get_child(action_button, 0);
}
