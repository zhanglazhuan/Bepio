#ifndef BEPIO_MN316_MANAGER_H_
#define BEPIO_MN316_MANAGER_H_

#include <stddef.h>
#include "bepio_mn316_service.h"

/* Synchronous, bounded APIs. Call from a task, never from an LVGL callback or ISR.
 * The service serializes UART1 access; a TLS session owns it until completion.
 */
int bepio_mn316_manager_init(void);
int bepio_mn316_manager_at(const char *command, char *reply, size_t capacity, int timeout_ms);
int bepio_mn316_manager_network_wait(int timeout_ms);
int bepio_mn316_manager_onenet_login(const char *device, const char *key_base64,
					const char *expires);
int bepio_mn316_manager_mqtt_subscribe(const char *topic);
int bepio_mn316_manager_mqtt_publish(const char *topic, const char *payload);
int bepio_mn316_manager_tls_test(const bepio_mn316_tls_credentials_t *credentials,
				int listen_seconds);

#endif
