#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bepio_storage_info.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "nvs.h"
static const esp_partition_t app = {.address = 0x10000, .size = 0x200000};
static const esp_partition_t files = {.address = 0x210000, .size = 0xdf0000};
static bool mounted, missing_files, missing_app;
static int flash_error, metadata_error, fs_error, nvs_error;
static uint32_t image_size = 1200000;
static size_t files_total = 13000000, files_used = 12345;
static unsigned fs_queries;
int esp_flash_get_size(void *chip, uint32_t *size) { (void)chip; *size = 16 * 1024 * 1024; return flash_error; }
const esp_partition_t *esp_ota_get_running_partition(void) { return missing_app ? NULL : &app; }
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label)
{
	assert(type == ESP_PARTITION_TYPE_DATA && subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS);
	assert(!strcmp(label, "storage")); return missing_files ? NULL : &files;
}
int esp_image_get_metadata(const esp_partition_pos_t *part, esp_image_metadata_t *data)
{
	assert(part->offset == app.address && part->size == app.size);
	data->image_len = image_size; return metadata_error;
}
bool esp_spiffs_mounted(const char *label) { assert(!strcmp(label, "storage")); return mounted; }
int esp_spiffs_info(const char *label, size_t *total, size_t *used)
{
	assert(!strcmp(label, "storage")); fs_queries++; *total = files_total; *used = files_used; return fs_error;
}
int nvs_get_stats(const char *part, nvs_stats_t *stats)
{
	assert(!part); *stats = (nvs_stats_t){.used_entries = 43, .total_entries = 756}; return nvs_error;
}
int main(void)
{
	bepio_storage_info_t info;
	assert(bepio_storage_info(NULL) == -EINVAL);
	assert(bepio_storage_info(&info) == 0);
	assert(info.flash_bytes == 16 * 1024 * 1024 && info.app_valid && info.app_used == image_size);
	assert(info.app_capacity == app.size && info.files_capacity == files.size);
	assert(!info.files_valid && info.files_error == -ENODEV && fs_queries == 0);
	assert(info.nvs_valid && info.nvs_used_entries == 43);
	mounted = true; assert(bepio_storage_info(&info) == 0);
	assert(info.files_valid && info.files_used == 12345 && info.files_total == 13000000);
	files_used = files_total + 1; assert(bepio_storage_info(&info) == 0 && !info.files_valid && info.files_error == -EIO);
	files_used = 1; files_total = files.size + 1; assert(bepio_storage_info(&info) == 0 && !info.files_valid);
	files_total = 13000000; fs_error = 1; assert(bepio_storage_info(&info) == 0 && !info.files_valid);
	missing_files = true; assert(bepio_storage_info(&info) == 0 && info.files_error == -ENOENT);
	metadata_error = 1; assert(bepio_storage_info(&info) == 0 && !info.app_valid && info.app_capacity == app.size);
	metadata_error = 0; image_size = app.size + 1; assert(bepio_storage_info(&info) == 0 && !info.app_valid);
	missing_app = true; nvs_error = 1; assert(bepio_storage_info(&info) == 0 && !info.app_valid && !info.nvs_valid);
	flash_error = 1; assert(bepio_storage_info(&info) == -EIO);
	puts("PASS read-only flash snapshot: actual firmware length, partition capacity, mounted/unmounted FS, invalid counters and read errors");
}
