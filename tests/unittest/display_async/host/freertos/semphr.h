#include "FreeRTOS.h"
typedef struct mock_semaphore *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *wake);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks);
