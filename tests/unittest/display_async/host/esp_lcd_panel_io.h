#ifndef MOCK_PANEL_IO_H_
#define MOCK_PANEL_IO_H_
#include <stdbool.h>
#include "esp_err.h"
typedef void *esp_lcd_panel_io_handle_t;
typedef struct { int unused; } esp_lcd_panel_io_event_data_t;
typedef bool (*mock_done_cb)(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *);
typedef struct { int dc_gpio_num, cs_gpio_num, pclk_hz, spi_mode, lcd_cmd_bits, lcd_param_bits, trans_queue_depth; mock_done_cb on_color_trans_done; } esp_lcd_panel_io_spi_config_t;
esp_err_t esp_lcd_new_panel_io_spi(int host, const esp_lcd_panel_io_spi_config_t *config, esp_lcd_panel_io_handle_t *io);
#endif
