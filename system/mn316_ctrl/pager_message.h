#ifndef BEPIO_PAGER_MESSAGE_H_
#define BEPIO_PAGER_MESSAGE_H_

#include <stdbool.h>
#include <stddef.h>

#define BEPIO_PAGER_NAME_MAX 64
#define BEPIO_PAGER_TEXT_MAX 384
#define BEPIO_PAGER_ADDRESS_MAX 192
#define BEPIO_PAGER_WIRE_MAX 960
#include "bepio_mn316_config.h"

struct bepio_pager_message {
	bool is_notification;
	char app[65];
	char title[97];
	char id[40]; /* UUID v4 or legacy:<32 hex>; stable across retries. */
	char from[BEPIO_PAGER_NAME_MAX + 1];
	char to[BEPIO_PAGER_NAME_MAX + 1];
	char text[BEPIO_PAGER_TEXT_MAX + 1];
	char datetime[26]; /* ISO 8601: UTC Z or explicit +/-HH:MM */
	bool has_location;
	double latitude;
	double longitude;
	char address[BEPIO_PAGER_ADDRESS_MAX + 1];
};

/* Missing Version/Type/Location default to 1/"message"/null.
 * Encoding always emits these defaults explicitly. Missing Id is derived from [From, To, Text, Datetime] with SHA-256.
 * From/To/Text/Datetime remain required. */
int bepio_pager_message_parse(const unsigned char *data, size_t size,
			      struct bepio_pager_message *message);
int bepio_pager_message_encode(const struct bepio_pager_message *message,
			       char *json, size_t capacity);
/* Diagnostic/new wire messages only. Never call when retrying a stored message. */
int bepio_pager_message_stamp_new(struct bepio_pager_message *message);

#endif
