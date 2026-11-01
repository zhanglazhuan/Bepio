#ifndef HOST_ESP_LOG_H_
#define HOST_ESP_LOG_H_
#include <stdio.h>
#define ESP_LOGI(tag, ...) do { printf("%s: ", tag); printf(__VA_ARGS__); puts(""); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define ESP_LOGD(tag, ...) ((void)0)
#endif
