/* Diagnostic single-master software I2C. Calls serialized by audio_worker.
 * Open drain only: logical 1 releases the line, never drives it high.
 * No automatic retries; failures retain byte/bit and ACK-stage evidence.
 */
#include <stdlib.h>
#include "audio_soft_i2c.h"
#include "bepio_audio.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define SDA BEPIO_AUDIO_SDA_GPIO
#define SCL BEPIO_AUDIO_SCL_GPIO
#define HALF_US 5
#define STRETCH_US 1000
static const char *TAG = "audio_soft_i2c";
static bool initialized;
typedef struct {
	audio_codec_ctrl_if_t base;
	uint8_t address;
	bool opened;
} soft_ctrl_t;

static void delay(void) { esp_rom_delay_us(HALF_US); }
static esp_err_t clock_high(void)
{
	gpio_set_level(SCL, 1);
	int64_t deadline = esp_timer_get_time() + STRETCH_US;
	while (!gpio_get_level(SCL)) {
		if (esp_timer_get_time() >= deadline) { return ESP_ERR_TIMEOUT; }
		esp_rom_delay_us(1);
	}
	delay();
	return ESP_OK;
}

static esp_err_t start(void)
{
	gpio_set_level(SDA, 1);
	delay();
	esp_err_t ret = clock_high();
	if (ret != ESP_OK) { return ret; }
	if (!gpio_get_level(SDA)) { return ESP_ERR_INVALID_STATE; }
	gpio_set_level(SDA, 0);
	delay();
	gpio_set_level(SCL, 0);
	return ESP_OK;
}

static esp_err_t stop(void)
{
	gpio_set_level(SCL, 0);
	gpio_set_level(SDA, 0);
	delay();
	esp_err_t ret = clock_high();
	gpio_set_level(SDA, 1);
	delay();
	if (ret == ESP_OK && !gpio_get_level(SDA)) { ret = ESP_ERR_INVALID_STATE; }
	return ret;
}

static esp_err_t write_byte(uint8_t byte, const char *stage)
{
	for (int bit = 7; bit >= 0; bit--) {
		gpio_set_level(SDA, (byte >> bit) & 1);
		delay();
		esp_err_t ret = clock_high();
		if (ret != ESP_OK) { return ret; }
		/* Observe the exact discrepancy seen in the hardware-I2C capture. */
		if ((byte & (1U << bit)) && !gpio_get_level(SDA)) {
			ESP_LOGE(TAG, "%s byte=0x%02x bit=%d sent 1 but SDA reads 0", stage, byte, bit);
			gpio_set_level(SCL, 0);
			return ESP_ERR_INVALID_RESPONSE;
		}
		gpio_set_level(SCL, 0);
	}
	gpio_set_level(SDA, 1);
	delay();
	esp_err_t ret = clock_high();
	if (ret != ESP_OK) { return ret; }
	bool ack = !gpio_get_level(SDA);
	gpio_set_level(SCL, 0);
	if (!ack) {
		ESP_LOGW(TAG, "%s byte=0x%02x NACK", stage, byte);
		return ESP_ERR_NOT_FOUND;
	}
	return ESP_OK;
}

static esp_err_t read_byte(uint8_t *out, bool ack)
{
	uint8_t value = 0;
	gpio_set_level(SDA, 1);
	for (unsigned bit = 0; bit < 8; bit++) {
		delay();
		esp_err_t ret = clock_high();
		if (ret != ESP_OK) { return ret; }
		value = (value << 1) | gpio_get_level(SDA);
		gpio_set_level(SCL, 0);
	}
	gpio_set_level(SDA, ack ? 0 : 1);
	delay();
	esp_err_t ret = clock_high();
	gpio_set_level(SCL, 0);
	gpio_set_level(SDA, 1);
	if (ret == ESP_OK) { *out = value; }
	return ret;
}

esp_err_t bepio_audio_soft_i2c_init(void)
{
	if (initialized) { return ESP_ERR_INVALID_STATE; }
	/* Disconnect any previous peripheral routing before software takes ownership. */
	esp_err_t ret = gpio_reset_pin(SDA);
	if (ret == ESP_OK) { ret = gpio_reset_pin(SCL); }
	if (ret != ESP_OK) { return ret; }
	gpio_set_level(SDA, 1); gpio_set_level(SCL, 1);
	gpio_config_t cfg = {.pin_bit_mask = (1ULL << SDA) | (1ULL << SCL),
		.mode = GPIO_MODE_INPUT_OUTPUT_OD, .pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE, .intr_type = GPIO_INTR_DISABLE};
	ret = gpio_config(&cfg);
	if (ret != ESP_OK) { return ret; }
	initialized = true;
	ESP_LOGI(TAG, "Software I2C SDA%d/SCL%d, open drain, half-period >=5us, SCL wait <=1ms", SDA, SCL);
	return ESP_OK;
}

void bepio_audio_soft_i2c_deinit(void)
{
	if (initialized) {
		gpio_reset_pin(SDA); gpio_reset_pin(SCL);
		initialized = false;
	}
}

esp_err_t bepio_audio_soft_i2c_probe(uint8_t address)
{
	if (!initialized || address > 0x7f) { return ESP_ERR_INVALID_STATE; }
	esp_err_t ret = start();
	if (ret == ESP_OK) { ret = write_byte(address << 1, "probe address"); }
	esp_err_t end = stop();
	return ret == ESP_OK ? end : ret;
}

static bool ctrl_is_open(const audio_codec_ctrl_if_t *ctrl)
{
	return ctrl && ((const soft_ctrl_t *)ctrl)->opened;
}

static int ctrl_info(const audio_codec_ctrl_if_t *ctrl, audio_codec_ctrl_info_t *info)
{
	if (!ctrl || !info) { return ESP_CODEC_DEV_INVALID_ARG; }
	*info = (audio_codec_ctrl_info_t){.type = AUDIO_CODEC_CTRL_I2C,
		.i2c = {.addr = ((const soft_ctrl_t *)ctrl)->address << 1, .port = BEPIO_AUDIO_I2C_PORT}};
	return ESP_CODEC_DEV_OK;
}

static int ctrl_close(const audio_codec_ctrl_if_t *ctrl)
{
	if (!ctrl) { return ESP_CODEC_DEV_INVALID_ARG; }
	((soft_ctrl_t *)ctrl)->opened = false;
	return ESP_CODEC_DEV_OK;
}

static int transfer(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len,
		    void *data, int len, bool reading)
{
	if (!ctrl || !data || reg_len != 1 || reg < 0 || reg > 255 || len < 1 || len > 4) {
		return ESP_CODEC_DEV_INVALID_ARG;
	}
	if (!ctrl_is_open(ctrl) || !initialized) { return ESP_CODEC_DEV_WRONG_STATE; }
	const soft_ctrl_t *dev = (const soft_ctrl_t *)ctrl;
	uint8_t *bytes = data;
	esp_err_t ret = start();
	if (ret == ESP_OK) { ret = write_byte(dev->address << 1, "write address"); }
	if (ret == ESP_OK) { ret = write_byte(reg, "register"); }
	if (reading) {
		if (ret == ESP_OK) { ret = start(); }
		if (ret == ESP_OK) { ret = write_byte((dev->address << 1) | 1, "read address"); }
		for (int i = 0; i < len && ret == ESP_OK; i++) { ret = read_byte(bytes + i, i + 1 < len); }
	} else {
		for (int i = 0; i < len && ret == ESP_OK; i++) { ret = write_byte(bytes[i], "data"); }
	}
	esp_err_t end = stop();
	if (ret == ESP_OK) { ret = end; }
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "%s addr=%02x reg=%02x: %s SDA=%d SCL=%d",
			reading ? "Read" : "Write", dev->address, reg, esp_err_to_name(ret),
			gpio_get_level(SDA), gpio_get_level(SCL));
		return reading ? ESP_CODEC_DEV_READ_FAIL : ESP_CODEC_DEV_WRITE_FAIL;
	}
	return ESP_CODEC_DEV_OK;
}
static int ctrl_read(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int len)
{
	return transfer(ctrl, reg, reg_len, data, len, true);
}
static int ctrl_write(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int len)
{
	return transfer(ctrl, reg, reg_len, data, len, false);
}

const audio_codec_ctrl_if_t *bepio_audio_soft_i2c_ctrl(uint8_t address)
{
	if (!initialized || address > 0x7f) { return NULL; }
	soft_ctrl_t *ctrl = calloc(1, sizeof(*ctrl));
	if (!ctrl) { return NULL; }
	ctrl->address = address; ctrl->opened = true;
	ctrl->base = (audio_codec_ctrl_if_t){.is_open = ctrl_is_open,
		.read_reg = ctrl_read, .write_reg = ctrl_write, .get_info = ctrl_info, .close = ctrl_close};
	return &ctrl->base;
}
