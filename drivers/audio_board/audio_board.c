#include "audio_board.h"
#include "audio_soft_i2c.h"
#include "bepio_audio.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const audio_codec_ctrl_if_t *bepio_audio_new_i2c_ctrl(audio_codec_i2c_cfg_t *config);

/* Clock/order reference: WeActStudio ES8311 example, commit 7294d8730b64. */
#define BEPIO_AUDIO_MCLK_MULTIPLE 384

static const char *TAG = "audio_board";
static i2c_master_bus_handle_t bus;
static i2s_chan_handle_t tx, rx;
static const audio_codec_ctrl_if_t *control;
static const audio_codec_data_if_t *data;
static const audio_codec_gpio_if_t *gpio;
static const audio_codec_if_t *codec_if;
static esp_codec_dev_handle_t codec;
static int codec_address = -1;
static bool open_attempted;

void bepio_audio_board_deinit(void)
{
	bool channels_closed = open_attempted;
	if (codec) {
		bepio_audio_board_mute(true);
		esp_codec_dev_close(codec);
		esp_codec_dev_delete(codec);
		codec = NULL;
	}
	if (codec_if) { audio_codec_delete_codec_if(codec_if); codec_if = NULL; }
	if (data) { audio_codec_delete_data_if(data); data = NULL; }
	if (gpio) { audio_codec_delete_gpio_if(gpio); gpio = NULL; }
	if (control) { audio_codec_delete_ctrl_if(control); control = NULL; }
	if (rx) {
		if (!channels_closed) { i2s_channel_disable(rx); }
		i2s_del_channel(rx);
		rx = NULL;
	}
	if (tx) {
		if (!channels_closed) { i2s_channel_disable(tx); }
		i2s_del_channel(tx);
		tx = NULL;
	}
	if (bus) { i2c_del_master_bus(bus); bus = NULL; }
#ifdef BEPIO_AUDIO_SOFT_I2C
	bepio_audio_soft_i2c_deinit();
#endif
	codec_address = -1;
	open_attempted = false;
}

esp_err_t bepio_audio_board_init(bool digital_mic, int volume, int gain_db)
{
	esp_err_t ret = ESP_OK;
	bepio_audio_board_deinit();
	i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(
		BEPIO_AUDIO_I2S_PORT, I2S_ROLE_MASTER);
	channel_config.dma_desc_num = BEPIO_AUDIO_TEST_DMA_COUNT;
	channel_config.dma_frame_num = BEPIO_AUDIO_TEST_FRAMES;
	channel_config.auto_clear = true;
	i2s_std_config_t config = {
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BEPIO_AUDIO_TEST_RATE),
		.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
			BEPIO_AUDIO_SAMPLE_BITS, I2S_SLOT_MODE_STEREO),
		.gpio_cfg = {
			.mclk = BEPIO_AUDIO_MCLK_GPIO,
			.bclk = BEPIO_AUDIO_BCLK_GPIO,
			.ws = BEPIO_AUDIO_LRCK_GPIO,
			.dout = BEPIO_AUDIO_TX_GPIO,
			.din = BEPIO_AUDIO_RX_GPIO,
		},
	};
	config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
	ESP_GOTO_ON_ERROR(i2s_new_channel(&channel_config, &tx, &rx), fail, TAG, "I2S channels");
	ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(tx, &config), fail, TAG, "I2S TX");
	ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(rx, &config), fail, TAG, "I2S RX");
	ESP_GOTO_ON_ERROR(i2s_channel_enable(tx), fail, TAG, "TX clock start");
	ESP_GOTO_ON_ERROR(i2s_channel_enable(rx), fail, TAG, "RX clock start");
	ESP_LOGI(TAG, "WeAct sequence: I2S started first, 16kHz MCLK=6.144MHz (384x), MCLK GPIO7");
#ifdef BEPIO_AUDIO_SOFT_I2C
	ESP_GOTO_ON_ERROR(bepio_audio_soft_i2c_init(), fail, TAG, "Software I2C init");
#else
	ESP_LOGI(TAG, "Native hardware I2C0 SDA41/SCL42 at 100kHz; software I2C disabled");
	i2c_master_bus_config_t bus_config = {
		.i2c_port = BEPIO_AUDIO_I2C_PORT,
		.sda_io_num = BEPIO_AUDIO_SDA_GPIO,
		.scl_io_num = BEPIO_AUDIO_SCL_GPIO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_GOTO_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), fail, TAG, "I2C init");
#endif
	/* Give the module supply time to settle before the first transaction. */
	vTaskDelay(pdMS_TO_TICKS(300));
	ESP_LOGI(TAG, "I2C%d SDA%d=%d SCL%d=%d; probing ES8311 0x18/0x19",
		BEPIO_AUDIO_I2C_PORT, BEPIO_AUDIO_SDA_GPIO,
		gpio_get_level(BEPIO_AUDIO_SDA_GPIO), BEPIO_AUDIO_SCL_GPIO,
		gpio_get_level(BEPIO_AUDIO_SCL_GPIO));
	/* Probe both CE straps. Prefer the address declared by the board. */
	int addresses[] = {BEPIO_AUDIO_CODEC_ADDR, BEPIO_AUDIO_CODEC_ADDR ^ 1};
	for (size_t i = 0; i < 2; i++) {
#ifdef BEPIO_AUDIO_SOFT_I2C
		ret = bepio_audio_soft_i2c_probe(addresses[i]);
#else
		ret = i2c_master_probe(bus, addresses[i], 100);
		if (ret == ESP_ERR_TIMEOUT) {
			ESP_LOGW(TAG, "Probe 0x%02x timed out: SDA=%d SCL=%d; recovering bus once",
				addresses[i], gpio_get_level(BEPIO_AUDIO_SDA_GPIO),
				gpio_get_level(BEPIO_AUDIO_SCL_GPIO));
			ret = i2c_master_bus_reset(bus);
			if (ret != ESP_OK) { goto fail; }
			vTaskDelay(pdMS_TO_TICKS(20));
			ret = i2c_master_probe(bus, addresses[i], 100);
		}
#endif
		ESP_LOGI(TAG, "Probe 0x%02x: %s", addresses[i], esp_err_to_name(ret));
		if (ret == ESP_OK) {
			codec_address = addresses[i];
			break;
		}
		if (ret != ESP_ERR_NOT_FOUND) { goto fail; }
	}
	if (codec_address < 0) {
		ESP_LOGE(TAG, "No ES8311 ACK at 0x18/0x19; check power, SDA/SCL and pull-ups");
		ret = ESP_ERR_NOT_FOUND;
		goto fail;
	}
#ifdef BEPIO_AUDIO_SOFT_I2C
	control = bepio_audio_soft_i2c_ctrl(codec_address);
#else
	audio_codec_i2c_cfg_t control_config = {
		.port = BEPIO_AUDIO_I2C_PORT,
		.addr = codec_address << 1, /* esp_codec_dev expects an 8-bit address. */
		.bus_handle = bus,
		.clock_speed_hz = BEPIO_AUDIO_I2C_FREQ_HZ,
	};
	control = bepio_audio_new_i2c_ctrl(&control_config);
#endif
	audio_codec_i2s_cfg_t data_config = {
		.port = BEPIO_AUDIO_I2S_PORT, .tx_handle = tx, .rx_handle = rx,
	};
	data = audio_codec_new_i2s_data(&data_config);
	gpio = audio_codec_new_gpio();
	if (!control || !data || !gpio) { ret = ESP_ERR_NO_MEM; goto fail; }
	es8311_codec_cfg_t codec_config = {
		.ctrl_if = control, .gpio_if = gpio,
		.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
		.pa_pin = -1, /* This module exposes no PA enable pin. */
		.master_mode = false, .use_mclk = true,
		.digital_mic = digital_mic, .no_dac_ref = false,
		.mclk_div = BEPIO_AUDIO_MCLK_MULTIPLE,
		.hw_gain = {.pa_voltage = 5.0f, .codec_dac_voltage = 3.3f},
	};
	ESP_LOGI(TAG, "=== CODEC register initialization BEGIN ===");
	codec_if = es8311_codec_new(&codec_config);
	if (!codec_if) { ret = ESP_FAIL; goto fail; }
	ESP_LOGI(TAG, "=== CODEC register initialization END ===");
	esp_codec_dev_cfg_t device_config = {
		.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT, .codec_if = codec_if, .data_if = data,
	};
	codec = esp_codec_dev_new(&device_config);
	if (!codec) { ret = ESP_ERR_NO_MEM; goto fail; }
	esp_codec_dev_sample_info_t format = {
		.sample_rate = BEPIO_AUDIO_TEST_RATE,
		.bits_per_sample = BEPIO_AUDIO_SAMPLE_BITS, .channel = 2, .channel_mask = 3,
		.mclk_multiple = BEPIO_AUDIO_MCLK_MULTIPLE,
	};
	ESP_LOGI(TAG, "=== CODEC format and enable BEGIN ===");
	open_attempted = true;
	ESP_GOTO_ON_ERROR(esp_codec_dev_open(codec, &format), fail, TAG, "Codec format");
	ESP_LOGI(TAG, "=== CODEC format and enable END ===");
	ESP_GOTO_ON_ERROR(bepio_audio_board_volume(volume), fail, TAG, "DAC volume");
	ESP_GOTO_ON_ERROR(bepio_audio_board_gain(gain_db), fail, TAG, "ADC gain");
	ESP_GOTO_ON_ERROR(bepio_audio_board_mute(true), fail, TAG, "Idle mute");
	vTaskDelay(pdMS_TO_TICKS(100));
	ESP_LOGI(TAG, "ES8311 ACK=0x%02x; %s mic; 16kHz/%dbit, MCLK=6.144MHz (384x)",
		 codec_address, digital_mic ? "digital" : "analog", BEPIO_AUDIO_SAMPLE_BITS);
	return ESP_OK;
fail:
	if (bus && codec_address < 0) {
		ESP_LOGE(TAG, "Codec probe failed: %s SDA%d=%d SCL%d=%d; check module supply/GND, wiring and 3.3V pull-ups",
			esp_err_to_name(ret), BEPIO_AUDIO_SDA_GPIO, gpio_get_level(BEPIO_AUDIO_SDA_GPIO),
			BEPIO_AUDIO_SCL_GPIO, gpio_get_level(BEPIO_AUDIO_SCL_GPIO));
	}
	bepio_audio_board_deinit();
	return ret;
}

/* The codec library owns setup; native transfers supply explicit timeouts and
 * verify byte counts. No other task accesses the codec or I2S handles.
 */
esp_err_t bepio_audio_board_read(int16_t *stereo, size_t frames)
{
	if (!stereo || !frames || frames > BEPIO_AUDIO_TEST_FRAMES) { return ESP_ERR_INVALID_ARG; }
#if BEPIO_AUDIO_SAMPLE_BITS == 32
	int32_t words[BEPIO_AUDIO_TEST_FRAMES * 2];
	size_t received = 0, bytes = frames * 2 * sizeof(int32_t);
	esp_err_t ret = i2s_channel_read(rx, words, bytes, &received, 250);
	if (ret != ESP_OK) { return ret; }
	if (received != bytes) { return ESP_ERR_INVALID_SIZE; }
	for (size_t i = 0; i < frames * 2; i++) { stereo[i] = (int16_t)(words[i] >> 16); }
	return ESP_OK;
#else
	size_t received = 0, bytes = frames * 2 * sizeof(int16_t);
	esp_err_t ret = i2s_channel_read(rx, stereo, bytes, &received, 250);
	return ret != ESP_OK ? ret : received == bytes ? ESP_OK : ESP_ERR_INVALID_SIZE;
#endif
}

esp_err_t bepio_audio_board_write(const int16_t *stereo, size_t frames)
{
	if (!stereo || !frames || frames > BEPIO_AUDIO_TEST_FRAMES) { return ESP_ERR_INVALID_ARG; }
#if BEPIO_AUDIO_SAMPLE_BITS == 32
	int32_t words[BEPIO_AUDIO_TEST_FRAMES * 2];
	for (size_t i = 0; i < frames * 2; i++) { words[i] = (int32_t)stereo[i] * 65536; }
	size_t sent = 0, bytes = frames * 2 * sizeof(int32_t);
	esp_err_t ret = i2s_channel_write(tx, words, bytes, &sent, 250);
#else
	size_t sent = 0, bytes = frames * 2 * sizeof(int16_t);
	esp_err_t ret = i2s_channel_write(tx, stereo, bytes, &sent, 250);
#endif
	return ret != ESP_OK ? ret : sent == bytes ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t bepio_audio_board_reset_input(void)
{
	ESP_RETURN_ON_ERROR(i2s_channel_disable(rx), TAG, "RX reset");
	return i2s_channel_enable(rx);
}

esp_err_t bepio_audio_board_reset_output(void)
{
	ESP_RETURN_ON_ERROR(i2s_channel_disable(tx), TAG, "TX reset");
#if BEPIO_AUDIO_SAMPLE_BITS == 32
	const int32_t silence[BEPIO_AUDIO_TEST_FRAMES * 2] = {0};
#else
	const int16_t silence[BEPIO_AUDIO_TEST_FRAMES * 2] = {0};
#endif
	for (int i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT; i++) {
		size_t loaded;
		ESP_RETURN_ON_ERROR(i2s_channel_preload_data(tx, silence, sizeof(silence), &loaded),
			TAG, "TX silence preload");
		if (loaded != sizeof(silence)) { return ESP_ERR_INVALID_SIZE; }
	}
	return i2s_channel_enable(tx);
}

esp_err_t bepio_audio_board_mute(bool mute)
{
	/* Use the concrete callbacks so I2C errors reach the diagnostic worker;
	 * the 1.6.2 high-level setters discard these callback return values.
	 */
	return codec && codec_if ? codec_if->mute(codec_if, mute) : ESP_ERR_INVALID_STATE;
}

esp_err_t bepio_audio_board_volume(int volume)
{
	if (volume < 0 || volume > 100) { return ESP_ERR_INVALID_ARG; }
	float db = volume ? (volume - 100) * 0.5f : -96.0f;
	return codec && codec_if ? codec_if->set_vol(codec_if, db) : ESP_ERR_INVALID_STATE;
}

esp_err_t bepio_audio_board_gain(int gain_db)
{
	if (gain_db < 0 || gain_db > 42 || gain_db % 6) { return ESP_ERR_INVALID_ARG; }
	return codec && codec_if ? codec_if->set_mic_gain(codec_if, gain_db) : ESP_ERR_INVALID_STATE;
}

esp_err_t bepio_audio_board_dump(void)
{
	static const uint8_t registers[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0d, 0x0e, 0x14, 0x16, 0x17, 0x31, 0x32, 0x44,
	};
	for (size_t i = 0; i < sizeof(registers); i++) {
		int value;
		ESP_RETURN_ON_ERROR(esp_codec_dev_read_reg(codec, registers[i], &value), TAG, "Register read");
		ESP_LOGI(TAG, "R%02x = %02x", registers[i], value);
	}
	return ESP_OK;
}

int bepio_audio_board_address(void)
{
	return codec_address;
}
