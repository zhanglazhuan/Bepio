#ifndef MOCK_FREERTOS_H_
#define MOCK_FREERTOS_H_
#include <stdint.h>
typedef unsigned TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((ms) / 10)
void mock_yield_from_isr(void);
#define portYIELD_FROM_ISR() mock_yield_from_isr()
#endif
