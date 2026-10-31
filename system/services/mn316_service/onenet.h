#ifndef BEPIO_ONENET_H_
#define BEPIO_ONENET_H_

#include <stddef.h>

#define BEPIO_ONENET_PRODUCT "2qX0myytsR"
#define BEPIO_ONENET_DEVICE "Bipeo_001"

int bepio_onenet_config(char *command, size_t capacity, const char *device);
int bepio_onenet_config_with_key(char *command, size_t capacity, const char *device,
				 const char *key_base64, const char *expires);

#endif
