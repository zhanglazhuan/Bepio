#ifndef WIFI_TEST_DRIVER_H_
#define WIFI_TEST_DRIVER_H_
#include "esp_event.h"
#include <stdbool.h>
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_STA 0
#define WIFI_IF_STA 0
#define WIFI_ALL_CHANNEL_SCAN 0
#define WPA3_SAE_PWE_BOTH 0
#define WIFI_SCAN_TYPE_ACTIVE 0
typedef enum { WIFI_AUTH_OPEN, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK,
 WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_UNSUPPORTED } wifi_auth_mode_t;
typedef struct { int unused; } wifi_init_config_t;
typedef struct { bool show_hidden; int scan_type; } wifi_scan_config_t;
typedef struct { uint8_t ssid[33], bssid[6], primary; int8_t rssi; wifi_auth_mode_t authmode; } wifi_ap_record_t;
typedef struct { struct { uint8_t ssid[32], password[64], bssid[6], channel, failure_retry_cnt;
 bool bssid_set; int scan_method; struct { bool capable; } pmf_cfg; int sae_pwe_h2e; } sta; } wifi_config_t;
typedef struct { uint16_t reason; } wifi_event_sta_disconnected_t;
int esp_wifi_init(const wifi_init_config_t *config);
int esp_wifi_set_storage(int value);
int esp_wifi_set_mode(int value);
int esp_wifi_deinit(void);
int esp_wifi_start(void);
int esp_wifi_stop(void);
int esp_wifi_disconnect(void);
int esp_wifi_connect(void);
int esp_wifi_set_config(int interface, const wifi_config_t *config);
int esp_wifi_scan_start(const wifi_scan_config_t *config, bool blocking);
int esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records);
int esp_wifi_clear_ap_list(void);
#endif
