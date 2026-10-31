#include "settings_about.h"
#include "settings_page_common.h"
#include "bepio_device_info.h"
void bepio_settings_about_render(void)
{
	char info[512];
	bepio_device_info(info, sizeof(info));
	bepio_settings_label(bepio_settings_ui.root, info, 12, 8, 296);
}
