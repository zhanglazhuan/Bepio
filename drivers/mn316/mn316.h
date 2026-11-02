#ifndef BEPIO_MN316_H_
#define BEPIO_MN316_H_

#include <stddef.h>
#include <stdint.h>

/* Transport knows UART, cellular network and verified TLS, never pager IDs,
 * MQTT topics, JSON messages or broker credentials. Stream lifetime is callback-only.
 * read: bytes, 0 EOF, -ETIMEDOUT/-EAGAIN, or other negative errno.
 */
typedef struct {
	void *context;
	int (*read)(void *context, unsigned char *data, size_t size);
	int (*write)(void *context, const unsigned char *data, size_t size);
} bepio_mn316_stream_t;

typedef struct {
	const char *host;
	uint16_t port;
	const unsigned char *ca;
	size_t ca_size;
	int restore_baud; /* 0 selects board default. */
} bepio_mn316_transport_config_t;

int bepio_mn316_driver_init(void);
int bepio_mn316_driver_at(const char *command, char *reply, size_t capacity, int timeout_ms);
int bepio_mn316_driver_network_wait(int timeout_ms);
int bepio_mn316_driver_tls(const bepio_mn316_transport_config_t *config,
	int (*session)(void *context, const bepio_mn316_stream_t *stream), void *context);

/* Probe under the driver UART lock. Recover a modem left at 115200 by an
 * interrupted TLS session and restore its normal 9600 baud.
 * Returns 0 on OK, -ETIMEDOUT on silence, -EIO on modem ERROR.
 */
int mn316_probe(void);

#endif
