#include "bepio_mn316_manager.h"
#include "bepio_mn316_service.h"

int bepio_mn316_manager_init(void)
{
	return bepio_mn316_service_init();
}

int bepio_mn316_manager_at(const char *command, char *reply, size_t capacity, int timeout_ms)
{
	return bepio_mn316_service_at(command, reply, capacity, timeout_ms);
}

int bepio_mn316_manager_network_wait(int timeout_ms)
{
	return bepio_mn316_service_network_wait(timeout_ms);
}

int bepio_mn316_manager_onenet_login(const char *device, const char *key_base64,
					const char *expires)
{
	return bepio_mn316_service_onenet_login(device, key_base64, expires);
}

int bepio_mn316_manager_mqtt_subscribe(const char *topic)
{
	return bepio_mn316_service_mqtt_subscribe(topic);
}

int bepio_mn316_manager_mqtt_publish(const char *topic, const char *payload)
{
	return bepio_mn316_service_mqtt_publish(topic, payload);
}

int bepio_mn316_manager_tls_test(const bepio_mn316_tls_credentials_t *credentials,
				int listen_seconds)
{
	return bepio_mn316_service_tls_test(credentials, listen_seconds);
}
