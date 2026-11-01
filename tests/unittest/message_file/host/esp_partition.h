#ifndef BEPIO_TEST_ESP_PARTITION_H_
#define BEPIO_TEST_ESP_PARTITION_H_
#include "esp_spiffs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 2
typedef struct { size_t size; } esp_partition_t;
extern uint8_t partition_byte;
static inline const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label)
{
	static const esp_partition_t partition = {.size = 513};
	(void)type; (void)subtype; (void)label;
	return &partition;
}
static inline esp_err_t esp_partition_read(const esp_partition_t *partition, size_t offset,
					  void *data, size_t size)
{
	if (offset + size > partition->size) { return -1; }
	memset(data, partition_byte, size);
	return ESP_OK;
}
#endif
