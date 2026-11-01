#ifndef BEPIO_MN316_NETWORK_H_
#define BEPIO_MN316_NETWORK_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Vendor states: 0 absent, 1 initialized, 2 disconnected, 3..7 connecting/connected. */
static inline int bepio_mn316_internal_mqtt_state(const char *reply)
{
	const char *line = strstr(reply, "+MQTTSTAT:");
	int state;
	return line && sscanf(line, "+MQTTSTAT: %d", &state) == 1 && state >= 0 && state <= 7
		? state : -1;
}

static inline bool bepio_mn316_registered(const char *reply)
{
	const char *line = strstr(reply, "+CEREG:");
	int mode, status;
	return line && sscanf(line, "+CEREG: %d,%d", &mode, &status) == 2 &&
		(status == 1 || status == 5);
}

static inline bool bepio_mn316_attached(const char *reply)
{
	const char *line = strstr(reply, "+CGATT:");
	int attached;
	return line && sscanf(line, "+CGATT: %d", &attached) == 1 && attached == 1;
}

static inline bool bepio_mn316_has_ip(const char *reply)
{
	const char *line = strstr(reply, "+CGPADDR:");
	char ip[16], tail;
	unsigned a, b, c, d;
	int cid;
	if (!line || (sscanf(line, "+CGPADDR: %d, \"%15[0-9.]\"", &cid, ip) != 2 &&
	    sscanf(line, "+CGPADDR: %d, %15[0-9.]", &cid, ip) != 2)) return false;
	return cid == 0 && sscanf(ip, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) == 4 &&
		a <= 255 && b <= 255 && c <= 255 && d <= 255 && (a || b || c || d);
}

#endif
