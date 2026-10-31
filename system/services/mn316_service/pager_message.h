#ifndef BEPIO_PAGER_MESSAGE_H_
#define BEPIO_PAGER_MESSAGE_H_

#include <stdbool.h>
#include <stddef.h>

#define BEPIO_PAGER_NAME_MAX 64
#define BEPIO_PAGER_TEXT_MAX 384
#define BEPIO_PAGER_ADDRESS_MAX 192
#define BEPIO_PAGER_WIRE_MAX 960
#define BEPIO_PAGER_DEVICE "Pager_MN316"

struct bepio_pager_message {
	char from[BEPIO_PAGER_NAME_MAX + 1];
	char to[BEPIO_PAGER_NAME_MAX + 1];
	char text[BEPIO_PAGER_TEXT_MAX + 1];
	char datetime[26]; /* ISO 8601: UTC Z or explicit +/-HH:MM */
	bool has_location;
	double latitude;
	double longitude;
	char address[BEPIO_PAGER_ADDRESS_MAX + 1];
};

int bepio_pager_message_parse(const unsigned char *data, size_t size,
			      struct bepio_pager_message *message);
int bepio_pager_message_encode(const struct bepio_pager_message *message,
			       char *json, size_t capacity);
int bepio_pager_message_now(struct bepio_pager_message *message);

#endif
