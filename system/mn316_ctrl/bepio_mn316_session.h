#ifndef BEPIO_MN316_SESSION_H_
#define BEPIO_MN316_SESSION_H_

#include <stdbool.h>
#include <stddef.h>
#include "pager_message.h"

typedef struct {
	void (*online)(bool connected);
	/* Return 1 with a message, 0 when idle, negative on error. */
	int (*take)(struct bepio_pager_message *message);
	void (*complete)(bool acknowledged);
	int (*receive)(const struct bepio_pager_message *message);
	void (*record)(size_t tx, size_t rx);
} bepio_mn316_mqtt_hooks_t;

typedef struct {
	const char *username;
	const char *password;
	const char *device_id;
	int uart_baud; /* 0 selects the board default. */
	/* NULL preserves the bounded diagnostic echo session. With hooks the
	 * application session runs until transport/heartbeat failure.
	 */
	const bepio_mn316_mqtt_hooks_t *hooks;
	const char *test_text; /* Optional initial diagnostic message; NULL uses default. */
} bepio_mn316_tls_credentials_t;

int bepio_mn316_session_run(const bepio_mn316_tls_credentials_t *credentials, int seconds);

#endif
