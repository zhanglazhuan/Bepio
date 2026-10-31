#ifndef WIFI_TEST_NETIF_H_
#define WIFI_TEST_NETIF_H_
#include "esp_event.h"
typedef struct { int unused; } esp_netif_t;
typedef struct { unsigned addr; } test_ip_t;
typedef struct { struct { test_ip_t ip; } ip_info; } ip_event_got_ip_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) ((void)(ip), 192u), 0u, 2u, 42u
int esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
#endif
