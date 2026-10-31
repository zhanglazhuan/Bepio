#ifndef WIFI_TEST_EVENT_H_
#define WIFI_TEST_EVENT_H_
#include <stdint.h>
typedef int esp_err_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
extern const char wifi_event_base[], ip_event_base[];
#define WIFI_EVENT wifi_event_base
#define IP_EVENT ip_event_base
#define WIFI_EVENT_STA_DISCONNECTED 1
#define IP_EVENT_STA_GOT_IP 2
#define IP_EVENT_STA_LOST_IP 3
int esp_event_loop_create_default(void);
int esp_event_handler_register(esp_event_base_t base, int32_t id, esp_event_handler_t cb, void *arg);
int esp_event_handler_unregister(esp_event_base_t base, int32_t id, esp_event_handler_t cb);
#endif
