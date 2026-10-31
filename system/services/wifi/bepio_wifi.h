#ifndef BEPIO_WIFI_H_
#define BEPIO_WIFI_H_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define BEPIO_WIFI_AP_MAX 16
#define BEPIO_WIFI_SSID_MAX 32
#define BEPIO_WIFI_PASSWORD_MAX 64
typedef struct {
	char ssid[BEPIO_WIFI_SSID_MAX + 1];
	uint8_t bssid[6], channel;
	int8_t rssi;
	bool secured, supported, saved;
} bepio_wifi_ap_t;
typedef struct {
	bool enabled, busy, scanning, connecting, connected;
	uint32_t revision;
	int error, storage_error;
	char ssid[BEPIO_WIFI_SSID_MAX + 1], ip[16], status[80];
	size_t count;
	bepio_wifi_ap_t aps[BEPIO_WIFI_AP_MAX];
} bepio_wifi_snapshot_t;
/* Issue requests on the UI owner thread (lazy initialization). Requests are nonblocking; snapshots never expose credentials. Radio starts only
 * after Enable. NULL password means use the previously saved credential. */
int bepio_wifi_set_enabled(bool enabled);
int bepio_wifi_scan(void);
int bepio_wifi_connect(const bepio_wifi_ap_t *ap, const char *password);
int bepio_wifi_disconnect(void);
void bepio_wifi_snapshot(bepio_wifi_snapshot_t *snapshot);
#endif
