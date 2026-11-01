/* Adapted from DeepStoa lv_toast for the 320x240 LCD. */
#include "lv_toast.h"
#include "lv_theme_hardcore.h"
#include "esp_log.h"

static void toast_deleted(lv_event_t *event)
{
	lv_timer_t *timer = lv_event_get_user_data(event);
	if (timer) { lv_timer_delete(timer); }
}

static void toast_expired(lv_timer_t *timer)
{
	lv_obj_t *toast = lv_timer_get_user_data(timer);
	lv_timer_delete(timer);
	if (toast) {
		lv_obj_remove_event_cb(toast, toast_deleted);
		lv_obj_delete(toast);
	}
}

void lv_toast_show(const char *message, int duration_ms)
{
	if (!message) { return; }
	lv_obj_t *toast = lv_label_create(lv_layer_top());
	if (!toast) { return; }
	lv_obj_set_style_bg_color(toast, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(toast, 191, 0);
	lv_obj_set_style_text_color(toast, lv_color_white(), 0);
	lv_obj_set_style_text_font(toast, LV_FONT_SMALL, 0);
	lv_obj_set_style_radius(toast, 8, 0);
	lv_obj_set_style_pad_hor(toast, 12, 0);
	lv_obj_set_style_pad_ver(toast, 8, 0);
	lv_obj_set_style_max_width(toast, lv_pct(90), 0);
	lv_label_set_long_mode(toast, LV_LABEL_LONG_WRAP);
	lv_label_set_text(toast, message);
	lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -24);
	lv_timer_t *timer = lv_timer_create(toast_expired,
		duration_ms > 0 ? (uint32_t)duration_ms : 1, toast);
	if (!timer) { lv_obj_delete(toast); return; }
	lv_obj_add_event_cb(toast, toast_deleted, LV_EVENT_DELETE, timer);
	ESP_LOGI("toast", "%s (%d ms)", message, duration_ms);
}
