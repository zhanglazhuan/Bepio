#ifndef BEPIO_LOG_H_
#define BEPIO_LOG_H_
#include "esp_log.h"
#define BEPIO_LOG_TAG(name) static const char *const TAG __attribute__((unused)) = #name
#define BEPIO_LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define BEPIO_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define BEPIO_LOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#endif
