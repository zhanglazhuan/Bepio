#include "FreeRTOS.h"
typedef void *EventGroupHandle_t;
typedef unsigned EventBits_t;
EventGroupHandle_t xEventGroupCreate(void);
void vEventGroupDelete(EventGroupHandle_t events);
unsigned xEventGroupSetBits(EventGroupHandle_t events, unsigned bits);
unsigned xEventGroupClearBits(EventGroupHandle_t events, unsigned bits);
unsigned xEventGroupWaitBits(EventGroupHandle_t events, unsigned bits, int clear, int all, unsigned timeout);
