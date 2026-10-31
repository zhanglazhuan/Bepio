#ifndef BEPIO_TEST_ESP_SPIFFS_H_
#define BEPIO_TEST_ESP_SPIFFS_H_
#include <stdbool.h>

typedef int esp_err_t;
#define ESP_OK 0
typedef struct {
	const char *base_path, *partition_label;
	int max_files;
	bool format_if_mount_failed;
} esp_vfs_spiffs_conf_t;
static inline esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *config)
{
	(void)config;
	return ESP_OK;
}
#endif
