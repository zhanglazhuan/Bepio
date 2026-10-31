#include "bepio_storage_info.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include <errno.h>
#include <string.h>
int bepio_storage_info(bepio_storage_info_t *info)
{
	if (!info) { return -EINVAL; }
	memset(info, 0, sizeof(*info));
	if (esp_flash_get_size(NULL, &info->flash_bytes) != ESP_OK || !info->flash_bytes) { return -EIO; }
	const esp_partition_t *app = esp_ota_get_running_partition();
	if (app) {
		info->app_capacity = app->size;
		esp_image_metadata_t metadata;
		esp_partition_pos_t position = {.offset = app->address, .size = app->size};
		if (esp_image_get_metadata(&position, &metadata) == ESP_OK && metadata.image_len > 0 && metadata.image_len <= app->size) {
			info->app_used = metadata.image_len; info->app_valid = true;
		}
	}
	const esp_partition_t *files = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
		ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
	if (!files) { info->files_error = -ENOENT; }
	else {
		info->files_capacity = files->size;
		if (!esp_spiffs_mounted("storage")) { info->files_error = -ENODEV; }
		else if (esp_spiffs_info("storage", &info->files_total, &info->files_used) != ESP_OK ||
			info->files_total > files->size || info->files_used > info->files_total || !info->files_total) {
			info->files_error = -EIO;
		} else { info->files_valid = true; }
	}
	nvs_stats_t stats;
	if (nvs_get_stats(NULL, &stats) == ESP_OK && stats.used_entries <= stats.total_entries) {
		info->nvs_used_entries = stats.used_entries;
		info->nvs_total_entries = stats.total_entries;
		info->nvs_valid = true;
	}
	return 0;
}
