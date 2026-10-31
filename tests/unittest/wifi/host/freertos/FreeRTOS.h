#ifndef WIFI_TEST_FREERTOS_H_
#define WIFI_TEST_FREERTOS_H_
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int BaseType_t;
typedef unsigned TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define BIT0 1u
#define BIT1 2u
#define BIT2 4u
#endif
