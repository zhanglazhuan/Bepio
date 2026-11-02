#include "about_info.h"
#include "bepio_account.h"
#include "bepio_device_info.h"
#include <stdio.h>

void bepio_settings_about_info(char *buffer, size_t size)
{
	if (!buffer || !size) { return; }
	bepio_account_snapshot_t account;
	char platform[384];
	bepio_account_snapshot(&account);
	bepio_device_info(platform, sizeof(platform));
	snprintf(buffer, size, "Device ID: %s\n%s", account.id, platform);
}
