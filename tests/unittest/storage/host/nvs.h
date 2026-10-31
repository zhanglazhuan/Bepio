#include <stddef.h>
typedef struct { size_t used_entries, total_entries; } nvs_stats_t;
int nvs_get_stats(const char *part, nvs_stats_t *stats);
