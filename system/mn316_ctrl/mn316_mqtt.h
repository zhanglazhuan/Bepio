#ifndef BEPIO_MN316_MQTT_H_
#define BEPIO_MN316_MQTT_H_

#include <stdint.h>
#include "bepio_mn316_session.h"

/* Authenticated, subscribed MQTT connection supplied by the TLS adapter. */
typedef struct {
	void *context;
	const char *device_id;
	const char *inbox;
	int (*send)(void *context, unsigned char type, const unsigned char *body, size_t size);
	int (*receive)(void *context, unsigned char *type, unsigned char *body,
		       size_t *size, int timeout_ms);
	int64_t (*now_us)(void *context);
} bepio_mn316_mqtt_io_t;

int bepio_mn316_mqtt_session(const bepio_mn316_mqtt_io_t *io,
			     const bepio_mn316_mqtt_hooks_t *hooks);

#endif
