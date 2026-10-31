#ifndef BEPIO_STORE_H_
#define BEPIO_STORE_H_
#include <stddef.h>
/* Atomic NVS blob replacement. Returns -ENOENT for an absent key. */
int bepio_store_read(const char *space, const char *key, void *data, size_t *size);
int bepio_store_write(const char *space, const char *key, const void *data, size_t size);
#endif
