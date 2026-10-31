#include "settings_usage.h"
#include "settings_page_common.h"
#include <inttypes.h>

void bepio_settings_usage_refresh(void)
{
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	lv_label_set_text_fmt(bepio_settings_ui.summary,
		"TX: %" PRIu64 " bytes\nRX: %" PRIu64 " bytes\n"
		"Connected: %" PRIu64 " min %" PRIu64 " sec\nChange cooldown: %lu sec",
		account.tx_bytes, account.rx_bytes,
		account.connected_seconds / 60, account.connected_seconds % 60,
		(unsigned long)account.cooldown_seconds);
}

void bepio_settings_usage_render(void)
{
	lv_obj_t *root = bepio_settings_ui.root;
	lv_obj_set_style_pad_hor(root, 12, 0);
	lv_obj_set_style_pad_ver(root, 8, 0);
	lv_obj_set_style_pad_row(root, 12, 0);
	lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
	bepio_settings_ui.summary = bepio_settings_label(root, "", 0, 0, 0);
	lv_obj_set_width(bepio_settings_ui.summary, lv_pct(100));
	bepio_settings_usage_refresh();
	lv_obj_t *notice = bepio_settings_label(root,
		"JSON payload only, excludes carrier overhead.", 0, 0, 0);
	lv_obj_set_width(notice, lv_pct(100));
}
