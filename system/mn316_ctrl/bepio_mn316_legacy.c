#include "bepio_mn316_manager.h"
#include "mn316_internal.h"
#include "mn316_network.h"
#include "onenet.h"
#include "mbedtls/platform_util.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool mqtt_open;

int bepio_mn316_manager_onenet_login(const char *device, const char *key_base64,
					const char *expires)
{
	char command[512], reply[256];
	if (bepio_onenet_config_with_key(command, sizeof(command), device,
					key_base64, expires)) return -EINVAL;
	int ret = bepio_mn316_driver_acquire();
	if (ret) { mbedtls_platform_zeroize(command, sizeof(command)); return ret; }
	mqtt_open = false;
	ret = bepio_mn316_command_locked("ATE0", NULL, reply, sizeof(reply), 2000);
	if (!ret) ret = bepio_mn316_command_locked("AT+CGATT?", NULL, reply, sizeof(reply), 2000);
	if (!ret && !bepio_mn316_attached(reply)) ret = -ENETDOWN;
	if (!ret) ret = bepio_mn316_command_locked("AT+MQTTSTAT?", NULL, reply, sizeof(reply), 2000);
	if (!ret && !strstr(reply, "+MQTTSTAT: 0")) {
		/* A prior client can survive host reset. Cleanup is bounded. */
		bepio_mn316_command_locked("AT+MQTTDISC", NULL, reply, sizeof(reply), 5000);
		ret = bepio_mn316_command_locked("AT+MQTTDEL", NULL, reply, sizeof(reply), 5000);
	}
	if (!ret) ret = bepio_mn316_command_locked(command, NULL, reply, sizeof(reply), 5000);
	mbedtls_platform_zeroize(command, sizeof(command));
	if (!ret) ret = bepio_mn316_command_locked("AT+MQTTOPEN=1,1,0,0,0,\"\",\"\"",
					"+MQTTOPEN:OK", reply, sizeof(reply), 60000);
	mqtt_open = ret == 0;
	bepio_mn316_driver_release();
	return ret;
}

int bepio_mn316_manager_mqtt_subscribe(const char *topic)
{
	if (!topic || !topic[0] || strlen(topic) > 150 || strchr(topic, '"')) return -EINVAL;
	int ret = bepio_mn316_driver_acquire();
	if (ret) return ret;
	if (!mqtt_open) ret = -ENOTCONN;
	else {
		char command[192], reply[256];
		snprintf(command, sizeof(command), "AT+MQTTSUB=\"%s\",0", topic);
		ret = bepio_mn316_command_locked(command, NULL, reply, sizeof(reply), 5000);
		if (!ret) ret = bepio_mn316_command_locked(NULL, "+MQTTSUBACK:*", reply, sizeof(reply), 20000);
		if (!ret) {
			int packet, code;
			char *ack = strstr(reply, "+MQTTSUBACK:");
			if (!ack || sscanf(ack, "+MQTTSUBACK: %d,%d", &packet, &code) != 2 ||
			    code < 0 || code > 2) ret = -EIO;
		}
	}
	bepio_mn316_driver_release();
	return ret;
}

int bepio_mn316_manager_mqtt_publish(const char *topic, const char *payload)
{
	if (!topic || !payload || !topic[0] || strlen(topic) > 150 ||
	    strchr(topic, '"') || strlen(payload) > 350) return -EINVAL;
	int ret = bepio_mn316_driver_acquire();
	if (ret) return ret;
	if (!mqtt_open) ret = -ENOTCONN;
	else {
		char command[1024], reply[256];
		size_t size = strlen(payload);
		int used = snprintf(command, sizeof(command), "AT+MQTTPUB=\"%s\",1,0,0,%u,\"",
				    topic, (unsigned)size);
		if (used < 0 || (size_t)used + size * 2 + 2 > sizeof(command)) ret = -EMSGSIZE;
		else {
			static const char hex[] = "0123456789ABCDEF";
			for (size_t i = 0; i < size; i++) {
				unsigned char byte = (unsigned char)payload[i];
				command[used++] = hex[byte >> 4];
				command[used++] = hex[byte & 15];
			}
			command[used++] = '"'; command[used] = '\0';
			ret = bepio_mn316_command_locked(command, NULL, reply, sizeof(reply), 5000);
		}
		mbedtls_platform_zeroize(command, sizeof(command));
	}
	bepio_mn316_driver_release();
	return ret;
}

