#ifndef BEPIO_FT6336_H_
#define BEPIO_FT6336_H_
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#define BEPIO_FT6336_MAX_POINTS 2

typedef struct {
	i2c_port_num_t i2c_port;
	gpio_num_t sda, scl, rst, interrupt;
} bepio_ft6336_config_t;
typedef struct {
	uint8_t track_id, event;
	uint16_t x, y;
} bepio_ft6336_point_t;
typedef struct {
	uint8_t count;
	bepio_ft6336_point_t points[BEPIO_FT6336_MAX_POINTS];
} bepio_ft6336_frame_t;
/* Single device; serialize calls. Failed initialization releases the bus. */
esp_err_t bepio_ft6336_init(const bepio_ft6336_config_t *config);
esp_err_t bepio_ft6336_scan(bepio_ft6336_frame_t *frame);
#endif
