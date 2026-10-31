#include "bepio_device_info.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_flash.h"
#include <stdio.h>
void bepio_device_info(char *buffer, size_t size)
{
	const esp_app_desc_t *app = esp_app_get_description();
	esp_chip_info_t chip;
	esp_chip_info(&chip);
	uint8_t mac[6] = {0};
	esp_efuse_mac_get_default(mac);
	uint32_t flash = 0;
	esp_flash_get_size(NULL, &flash);
	snprintf(buffer, size, "%s  v%s\nESP32-S3 rev %u / %u cores\nFlash: %lu MB  Free heap: %lu KB\nIDF: %s\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\nBuilt: %s %s",
		app->project_name, app->version, chip.revision, chip.cores,
		(unsigned long)(flash / (1024 * 1024)), (unsigned long)(esp_get_free_heap_size() / 1024),
		app->idf_ver, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], app->date, app->time);
}
