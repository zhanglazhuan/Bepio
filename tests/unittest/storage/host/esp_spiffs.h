#include <stdbool.h>
#include <stddef.h>
bool esp_spiffs_mounted(const char *label);
int esp_spiffs_info(const char *label, size_t *total, size_t *used);
