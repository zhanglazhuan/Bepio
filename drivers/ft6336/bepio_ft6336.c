/* Protocol reference: CL28CK230-18A_CTP_230801 STM32F1 demo,
 * HARDWARE/TOUCH/ctp.c and ctp.h. Native ESP-IDF implementation.
 * 0x70/0x71 on the wire means 7-bit address 0x38, 8-bit registers.
 */
#include <string.h>
#include "bepio_ft6336.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ft6336";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;

static esp_err_t read_reg(uint8_t reg, uint8_t *data, size_t size)
{
	return i2c_master_transmit_receive(device, &reg, 1, data, size, 100);
}

esp_err_t bepio_ft6336_init(const bepio_ft6336_config_t *config)
{
	if (!config) { return ESP_ERR_INVALID_ARG; }
	if (bus) { return ESP_ERR_INVALID_STATE; }
	i2c_master_bus_config_t cfg = {.i2c_port = config->i2c_port,
		.sda_io_num = config->sda, .scl_io_num = config->scl,
		.clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true};
	esp_err_t ret = i2c_new_master_bus(&cfg, &bus);
	if (ret != ESP_OK) { return ret; }
	gpio_config_t rst = {.pin_bit_mask = 1ULL << config->rst, .mode = GPIO_MODE_OUTPUT};
	gpio_config_t interrupt = {.pin_bit_mask = 1ULL << config->interrupt, .mode = GPIO_MODE_INPUT};
	ret = gpio_config(&interrupt);
	if (ret == ESP_OK) { ret = gpio_config(&rst); }
	if (ret == ESP_OK) { ret = gpio_set_level(config->rst, 0); }
	if (ret != ESP_OK) { goto fail; }
	vTaskDelay(pdMS_TO_TICKS(20));
	ret = gpio_set_level(config->rst, 1);
	if (ret != ESP_OK) { goto fail; }
	vTaskDelay(pdMS_TO_TICKS(300));
	ESP_LOGI(TAG, "Probe 0x38 I2C%d SDA=%d SCL=%d RST=%d INT=%d; levels SDA=%d SCL=%d INT=%d",
		config->i2c_port, config->sda, config->scl, config->rst, config->interrupt,
		gpio_get_level(config->sda), gpio_get_level(config->scl), gpio_get_level(config->interrupt));
	ret = i2c_master_probe(bus, 0x38, 100);
	if (ret != ESP_OK) { goto fail; }
	i2c_device_config_t dev_cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x38, .scl_speed_hz = 100000};
	ret = i2c_master_bus_add_device(bus, &dev_cfg, &device);
	if (ret != ESP_OK) { goto fail; }
	uint8_t status;
	ret = read_reg(0x02, &status, 1);
	if (ret != ESP_OK) { goto fail; }
	/* Unsupported contact counts are filtered by scan, not an init failure. */
	ESP_LOGI(TAG, "FT6336-compatible device responds at 0x38; status=0x%02x", status);
	return ESP_OK;
fail:
	if (device) { i2c_master_bus_rm_device(device); device = NULL; }
	i2c_del_master_bus(bus); bus = NULL;
	return ret;
}

esp_err_t bepio_ft6336_scan(bepio_ft6336_frame_t *frame)
{
	if (!frame) { return ESP_ERR_INVALID_ARG; }
	memset(frame, 0, sizeof(*frame));
	if (!device) { return ESP_ERR_INVALID_STATE; }
	/* Status at 0x02, two contacts at 0x03/0x09, six bytes per contact. */
	uint8_t raw[13];
	esp_err_t ret = read_reg(0x02, raw, sizeof(raw));
	if (ret != ESP_OK) { return ret; }
	unsigned count = raw[0] & 0x0f;
	/* Ignore unsupported frames as released; do not parse partial contacts.
	 * Transport failures above still propagate to the caller. */
	if (count > BEPIO_FT6336_MAX_POINTS) { return ESP_OK; }
	for (unsigned i = 0; i < count; i++) {
		const uint8_t *p = raw + 1 + i * 6;
		unsigned event = p[0] >> 6;
		/* Report both initial DOWN and CONTACT; UP is represented by release. */
		if (event != 0 && event != 2) { continue; }
		frame->points[frame->count++] = (bepio_ft6336_point_t){
			.track_id = p[2] >> 4, .event = event,
			.x = ((uint16_t)(p[0] & 0x0f) << 8) | p[1],
			.y = ((uint16_t)(p[2] & 0x0f) << 8) | p[3]};
	}
	return ESP_OK;
}
