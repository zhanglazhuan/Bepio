#ifndef BEPIO_LCD_SDK_MOCK_H_
#define BEPIO_LCD_SDK_MOCK_H_
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
extern bool mock_pending, mock_auto_complete;
extern unsigned mock_isr_yields;
extern int mock_submissions, mock_heap_fail_after, mock_allocations;
extern esp_err_t mock_submit_error;
extern int64_t mock_now;
extern uint16_t mock_panel[240 * 320];
void mock_complete(void);
#endif
