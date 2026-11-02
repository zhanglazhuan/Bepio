#ifndef BEPIO_MESSAGE_UUID_H_
#define BEPIO_MESSAGE_UUID_H_

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_random.h"
#else
#include <stdlib.h>
#ifdef _WIN32
extern int rand_s(unsigned int *value);
#else
#include <stdio.h>
#endif
#endif

/* UUID v4, canonical lowercase representation, including the terminating NUL. */
static inline bool bepio_message_uuid_valid(const char *id)
{
	if (!id || strlen(id) != 36 || id[14] != '4' ||
	    (id[19] != '8' && id[19] != '9' && id[19] != 'a' && id[19] != 'b')) return false;
	for (size_t i = 0; i < 36; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			if (id[i] != '-') return false;
		} else if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
	}
	return true;
}

static inline int bepio_message_uuid_generate(char id[37])
{
	unsigned char bytes[16];
#ifdef ESP_PLATFORM
	esp_fill_random(bytes, sizeof(bytes));
#elif defined(_WIN32)
	for (size_t i = 0; i < sizeof(bytes); i += sizeof(unsigned int)) {
		unsigned int value;
		if (rand_s(&value)) return -1;
		memcpy(bytes + i, &value, sizeof(value));
	}
#else
	FILE *random = fopen("/dev/urandom", "rb");
	if (!random) return -1;
	size_t count = fread(bytes, 1, sizeof(bytes), random);
	fclose(random);
	if (count != sizeof(bytes)) return -1;
#endif
	bytes[6] = (bytes[6] & 15) | 0x40;
	bytes[8] = (bytes[8] & 63) | 0x80;
	static const char hex[] = "0123456789abcdef";
	size_t at = 0;
	for (size_t i = 0; i < sizeof(bytes); i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10) id[at++] = '-';
		id[at++] = hex[bytes[i] >> 4];
		id[at++] = hex[bytes[i] & 15];
	}
	id[at] = '\0';
	return 0;
}

#endif
