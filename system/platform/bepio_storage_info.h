#ifndef BEPIO_STORAGE_INFO_H_
#define BEPIO_STORAGE_INFO_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
	uint32_t flash_bytes, app_capacity, app_used, files_capacity;
	size_t files_total, files_used, nvs_used_entries, nvs_total_entries;
	bool app_valid, files_valid, nvs_valid;
	int files_error;
} bepio_storage_info_t;
/* Read-only snapshot: never mounts, formats, repairs or erases flash. */
int bepio_storage_info(bepio_storage_info_t *info);
#endif
