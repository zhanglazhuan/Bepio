#ifndef STORAGE_TEST_PARTITION_H_
#define STORAGE_TEST_PARTITION_H_
#include <stdint.h>
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 2
typedef struct { uint32_t address, size; } esp_partition_t;
const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label);
#endif
