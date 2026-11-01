/* Flash usage presentation inspired by NomadCast's Storage page. */
#include "settings_storage.h"
#include "settings_page_context.h"
#include "lv_page.h"
#include "lv_controls.h"
#include <stdio.h>
#include "bepio_storage_info.h"
#include <errno.h>
#ifdef CONFIG_BEPIO_APP_MESSAGES
#include "messaging.h"
#endif

typedef struct {
	settings_page_context_t base;
	lv_obj_t *choices[3];
} storage_context_t;
#ifdef CONFIG_BEPIO_APP_MESSAGES
static void retention_selected(lv_event_t *event)
{
	storage_context_t *ctx = lv_event_get_user_data(event);
	const unsigned values[] = {3, 7, 14};
	unsigned days = 0;
	for (size_t i = 0; i < 3; i++) {
		if (ctx->choices[i] == lv_event_get_target_obj(event)) { days = values[i]; break; }
	}
	if (!days) { return; }
	if (bepio_messaging_set_retention_days(days)) {
		bepio_ui_label(ctx->base.root, "Could not save message retention", 12, 302, 296);
		return;
	}
	for (size_t i = 0; i < 3; i++) {
		lv_obj_set_style_bg_color(ctx->choices[i],
			values[i] == days ? lv_color_hex(0x276640) : lv_color_hex(0x405d43), 0);
	}
}

static void retention_controls(storage_context_t *ctx)
{
	bepio_ui_label(ctx->base.root, "Delete chats older than", 12, 233, 296);
	const unsigned days[] = {3, 7, 14};
	for (size_t i = 0; i < 3; i++) {
		char label[16];
		snprintf(label, sizeof(label), "%u days", days[i]);
		lv_obj_t *choice = bepio_ui_button(ctx->base.root, label,
			12 + (int)i * 101, 258, 94, 32, retention_selected, ctx);
		ctx->choices[i] = choice;
		if (days[i] == bepio_messaging_retention_days()) {
			lv_obj_set_style_bg_color(choice, lv_color_hex(0x276640), 0);
		}
	}
}
#else
static void retention_controls(storage_context_t *ctx) { (void)ctx; }
#endif
static void size_text(char *buffer, size_t size, uint64_t bytes)
{
	uint64_t unit = bytes >= 1024 * 1024 ? 1024 * 1024 : 1024;
	unsigned hundredths = (unsigned)((bytes * 100 + unit / 2) / unit);
	snprintf(buffer, size, "%u.%02u %s", hundredths / 100, hundredths % 100,
		unit == 1024 ? "KiB" : "MiB");
}
static void usage_bar(storage_context_t *ctx, int y, size_t used, size_t total)
{
	lv_obj_t *bar = lv_bar_create(ctx->base.root);
	lv_obj_set_pos(bar, 12, y); lv_obj_set_size(bar, 296, 10);
	lv_bar_set_range(bar, 0, 100);
	unsigned percent = total ? (unsigned)((uint64_t)used * 100 / total) : 0;
	lv_bar_set_value(bar, percent, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(bar, lv_color_hex(percent >= 90 ? 0xc14545 : 0x405d43), LV_PART_INDICATOR);
}
lv_obj_t *bepio_settings_storage_build(void *app, void *payload)
{
	(void)payload;
	settings_view_t *view = app;
	const settings_model_t *model = view->model;
	Page page = lv_page_create("Storage", true, bepio_settings_page_back, NULL);
	if (!page.screen || !page.container) { return NULL; }
	bepio_settings_page_prepare(view, page, "Storage");
	storage_context_t *ctx = (storage_context_t *)bepio_settings_page_alloc(sizeof(*ctx), view, model);
	if (!ctx) { return bepio_settings_page_finish(view, page, NULL); }
	lv_obj_t *root = ctx->base.root;
	bepio_storage_info_t info;
	if (bepio_storage_info(&info)) {
		bepio_ui_label(root, "Flash information unavailable", 12, 12, 296);
		retention_controls(ctx); return bepio_settings_page_finish(view, page, &ctx->base);
	}
	char total[32], used[32], capacity[32], line[128];
	size_text(total, sizeof(total), info.flash_bytes);
	snprintf(line, sizeof(line), "Internal Flash: %s", total);
	bepio_ui_label(root, line, 12, 8, 296);
	size_text(capacity, sizeof(capacity), info.app_capacity);
	if (info.app_valid) {
		size_text(used, sizeof(used), info.app_used);
		snprintf(line, sizeof(line), "Firmware: %s / %s", used, capacity);
		usage_bar(ctx, 59, info.app_used, info.app_capacity);
	} else { snprintf(line, sizeof(line), "Firmware: unknown / %s", capacity); }
	bepio_ui_label(root, line, 12, 34, 296);
	if (info.files_valid) {
		size_text(total, sizeof(total), info.files_total); size_text(used, sizeof(used), info.files_used);
		snprintf(line, sizeof(line), "Files: %s / %s", used, total);
		bepio_ui_label(root, line, 12, 82, 296);
		usage_bar(ctx, 107, info.files_used, info.files_total);
		size_text(total, sizeof(total), info.files_total - info.files_used);
		snprintf(line, sizeof(line), "Available files: %s", total);
	} else {
		size_text(capacity, sizeof(capacity), info.files_capacity);
		snprintf(line, sizeof(line), "Files partition: %s", capacity);
		bepio_ui_label(root, line, 12, 82, 296);
		snprintf(line, sizeof(line), "%s", info.files_error == -ENODEV ? "Not mounted; usage unknown" :
			info.files_error == -ENOENT ? "No files partition" : "File usage unavailable");
	}
	bepio_ui_label(root, line, 12, 123, 296);
	if (info.nvs_valid) {
		snprintf(line, sizeof(line), "NVS: %u / %u entries used", (unsigned)info.nvs_used_entries,
			(unsigned)info.nvs_total_entries);
	} else { snprintf(line, sizeof(line), "NVS usage unavailable"); }
	bepio_ui_label(root, line, 12, 150, 296);
	bepio_ui_label(root, "Firmware space and file space are separate partitions.", 12, 178, 296);
	retention_controls(ctx);
	return bepio_settings_page_finish(view, page, &ctx->base);
}
