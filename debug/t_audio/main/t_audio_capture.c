/* Automatic logic-analyzer capture, SDA41/SCL42, no codec configuration writes. */
#include "bepio_audio.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_capture";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t codec;

static void levels(const char *label)
{
	ESP_LOGI(TAG, "%s SDA=%d SCL=%d", label,
		gpio_get_level(BEPIO_AUDIO_SDA_GPIO), gpio_get_level(BEPIO_AUDIO_SCL_GPIO));
}

static void phase(const char *name)
{
	ESP_LOGI(TAG, "=== %s ===", name);
	levels("Phase entry");
	/* Identical wire sequence per round: probe18, probe19, readFD, readFE.
	 * Keep failures visible; do not hide them with retries or alternate reads. */
	for (unsigned round = 1; round <= 3; round++) {
		for (unsigned address = 0x18; address <= 0x19; address++) {
			esp_err_t ret = i2c_master_probe(bus, address, 50);
			ESP_LOGI(TAG, "%s round=%u probe=0x%02x %s", name, round, address, esp_err_to_name(ret));
		}
		const uint8_t regs[] = {0xfd, 0xfe};
		for (unsigned i = 0; i < sizeof(regs); i++) {
			uint8_t value = 0;
			esp_err_t ret = i2c_master_transmit_receive(codec, &regs[i], 1, &value, 1, 50);
			if (ret == ESP_OK) {
				ESP_LOGI(TAG, "%s round=%u read18 reg=%02x value=%02x ID_%s", name, round,
					regs[i], value, value == (i == 0 ? 0x83 : 0x11) ? "OK" : "MISMATCH");
			} else {
				ESP_LOGW(TAG, "%s round=%u read18 reg=%02x %s", name, round, regs[i], esp_err_to_name(ret));
			}
		}
		levels("Round end");
		vTaskDelay(pdMS_TO_TICKS(150));
	}
}

void app_main(void)
{
	ESP_LOGI(TAG, "Capture mode: no commands, tone, recording or codec setup");
	ESP_LOGI(TAG, "I2C100kHz SDA41/SCL42; first START after 1.5s; phases A/B/C");
	i2c_master_bus_config_t bus_cfg = {
		.i2c_port = BEPIO_AUDIO_I2C_PORT, .sda_io_num = BEPIO_AUDIO_SDA_GPIO,
		.scl_io_num = BEPIO_AUDIO_SCL_GPIO, .clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
	i2c_device_config_t device_cfg = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x18, .scl_speed_hz = 100000};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device_cfg, &codec));
	vTaskDelay(pdMS_TO_TICKS(1500));
	phase("A clocks OFF");

	i2s_chan_handle_t tx = NULL, rx = NULL;
	i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BEPIO_AUDIO_I2S_PORT, I2S_ROLE_MASTER);
	channel_cfg.auto_clear = true;
	ESP_ERROR_CHECK(i2s_new_channel(&channel_cfg, &tx, &rx));
	i2s_std_config_t config = {
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
		.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
		.gpio_cfg = {.mclk = BEPIO_AUDIO_MCLK_GPIO, .bclk = BEPIO_AUDIO_BCLK_GPIO,
			.ws = BEPIO_AUDIO_LRCK_GPIO, .dout = BEPIO_AUDIO_TX_GPIO, .din = BEPIO_AUDIO_RX_GPIO},
	};
	config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
	ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &config));
	ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &config));
	ESP_ERROR_CHECK(i2s_channel_enable(tx));
	ESP_ERROR_CHECK(i2s_channel_enable(rx));
	ESP_LOGI(TAG, "Full clocks ON: MCLK=6.144MHz BCLK=512kHz LRCK=16kHz; silent DMA");
	vTaskDelay(pdMS_TO_TICKS(100));
	phase("B clocks ON");

	ESP_ERROR_CHECK(i2s_channel_disable(rx));
	ESP_ERROR_CHECK(i2s_channel_disable(tx));
	ESP_ERROR_CHECK(i2s_del_channel(rx));
	ESP_ERROR_CHECK(i2s_del_channel(tx));
	const gpio_num_t pins[] = {BEPIO_AUDIO_MCLK_GPIO, BEPIO_AUDIO_BCLK_GPIO,
		BEPIO_AUDIO_LRCK_GPIO, BEPIO_AUDIO_TX_GPIO, BEPIO_AUDIO_RX_GPIO};
	for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		ESP_ERROR_CHECK(gpio_reset_pin(pins[i]));
	}
	/* Recovery clocks may appear between B and C; explicitly mark in UART log. */
	ESP_LOGI(TAG, "Clocks OFF; I2C recovery before phase C");
	ESP_ERROR_CHECK(i2c_master_bus_reset(bus));
	vTaskDelay(pdMS_TO_TICKS(100));
	phase("C clocks OFF after recovery");
	ESP_LOGI(TAG, "Capture complete. RESET to repeat; full power cycle for a fresh codec state.");
	ESP_ERROR_CHECK(i2c_master_bus_rm_device(codec));
	ESP_ERROR_CHECK(i2c_del_master_bus(bus));
}
