#ifndef STORAGE_TEST_FLASH_H_
#define STORAGE_TEST_FLASH_H_
#include <stdint.h>
#define ESP_OK 0
int esp_flash_get_size(void *chip, uint32_t *size);
#endif
