#include <stdint.h>
typedef struct { uint32_t offset, size; } esp_partition_pos_t;
typedef struct { uint32_t image_len; } esp_image_metadata_t;
int esp_image_get_metadata(const esp_partition_pos_t *part, esp_image_metadata_t *metadata);
