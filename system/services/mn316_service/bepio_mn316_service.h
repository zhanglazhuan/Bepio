#ifndef BEPIO_MN316_SERVICE_H_
#define BEPIO_MN316_SERVICE_H_

#include <stddef.h>
#include "mn316_tls.h"

int bepio_mn316_service_init(void);
int bepio_mn316_service_at(const char *command, char *reply, size_t capacity, int timeout_ms);
int bepio_mn316_service_network_wait(int timeout_ms);
int bepio_mn316_service_onenet_login(const char *device, const char *key_base64,
					const char *expires);
int bepio_mn316_service_mqtt_subscribe(const char *topic);
int bepio_mn316_service_mqtt_publish(const char *topic, const char *payload);
int bepio_mn316_service_tls_test(const bepio_mn316_tls_credentials_t *credentials,
				int listen_seconds);

#endif
