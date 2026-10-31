#ifndef MOCK_PANEL_OPS_H_
#define MOCK_PANEL_OPS_H_
#include <stdbool.h>
#include "esp_err.h"
typedef void *esp_lcd_panel_handle_t;
esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel);
esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t panel, bool swap);
esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t panel, bool x, bool y);
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t panel, bool on);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x1, int y1, int x2, int y2, const void *pixels);
#endif
