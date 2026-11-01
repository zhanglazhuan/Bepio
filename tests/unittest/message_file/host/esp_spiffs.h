#ifndef BEPIO_TEST_ESP_SPIFFS_H_
#define BEPIO_TEST_ESP_SPIFFS_H_
#include <stdbool.h>

typedef int esp_err_t;
#define ESP_OK 0
extern int spiffs_fail_once;
extern int spiffs_format_calls;
extern int spiffs_unmount_calls;
typedef struct {
	const char *base_path, *partition_label;
	int max_files;
	bool format_if_mount_failed;
} esp_vfs_spiffs_conf_t;
static inline esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *config)
{
	if (config->format_if_mount_failed) { spiffs_format_calls++; return ESP_OK; }
	if (spiffs_fail_once) { spiffs_fail_once = 0; return -1; }
	return ESP_OK;
}
static inline esp_err_t esp_vfs_spiffs_unregister(const char *label)
{
	(void)label;
	spiffs_unmount_calls++;
	return ESP_OK;
}
#endif
