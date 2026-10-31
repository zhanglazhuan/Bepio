#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_board.h"
#include "audio_signal.h"
#include "bepio_audio.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define RECORD_SECONDS 3
#define RECORD_SAMPLES (BEPIO_AUDIO_TEST_RATE * RECORD_SECONDS)

typedef enum {
	JOB_IDLE, JOB_INIT, JOB_TONE, JOB_RECORD, JOB_PLAY, JOB_METER,
	JOB_VOLUME, JOB_GAIN, JOB_MIC, JOB_SLOT, JOB_REGS,
} audio_job_t;

typedef struct {
	audio_job_t job;
	int value;
	int seconds;
} audio_request_t;

static const char *TAG = "t_audio";
static const char *const job_names[] = {
	"idle", "init", "tone", "record", "play", "meter",
	"volume", "gain", "mic", "slot", "regs",
};
static int16_t recording[RECORD_SAMPLES]; /* 96 KB mono PCM, no PSRAM needed. */
static atomic_size_t recorded_samples;
static atomic_int current_job;
static atomic_int last_result;
static atomic_int volume = 60;
static atomic_int gain_db = 24;
static atomic_int input_slot;
static atomic_int address = -1;
static atomic_bool stop_requested;
static atomic_bool ready;
static atomic_bool digital_mic = true; /* Module manual: onboard digital microphone. */
static QueueHandle_t requests;

static void status(void)
{
	printf("ready=%d job=%s last=%s mic=%s slot=%s volume=%d gain=%ddB\n",
		atomic_load(&ready), job_names[atomic_load(&current_job)],
		esp_err_to_name(atomic_load(&last_result)),
		atomic_load(&digital_mic) ? "digital" : "analog",
		atomic_load(&input_slot) ? "right" : "left",
		atomic_load(&volume), atomic_load(&gain_db));
	size_t count = atomic_load(&recorded_samples);
	printf("recording=%u samples / %u ms [RAM-only]; ES8311 address=%d\n",
		(unsigned)count, (unsigned)(count * 1000 / BEPIO_AUDIO_TEST_RATE),
		atomic_load(&address));
	printf("SDA=%d SCL=%d MCLK=%d BCLK=%d LRCK=%d codec-DOUT/RX=%d codec-DIN/TX=%d\n",
		BEPIO_AUDIO_SDA_GPIO, BEPIO_AUDIO_SCL_GPIO, BEPIO_AUDIO_MCLK_GPIO,
		BEPIO_AUDIO_BCLK_GPIO, BEPIO_AUDIO_LRCK_GPIO,
		BEPIO_AUDIO_RX_GPIO, BEPIO_AUDIO_TX_GPIO);
}

static esp_err_t initialize(bool digital)
{
	atomic_store(&ready, false);
	atomic_store(&digital_mic, digital);
	esp_err_t ret = bepio_audio_board_init(digital, atomic_load(&volume), atomic_load(&gain_db));
	atomic_store(&address, bepio_audio_board_address());
	atomic_store(&ready, ret == ESP_OK);
	return ret;
}

static void log_meter(const char *label, const bepio_audio_meter_t *meter)
{
	double dc = meter->count ? (double)meter->sum / meter->count : 0;
	ESP_LOGI(TAG, "%s RMS=%.1f dBFS=%.1f DC=%.1f peak=%d clipped=%u/%u",
		 label, bepio_audio_meter_rms(meter), bepio_audio_meter_dbfs(meter), dc,
		 meter->peak, (unsigned)meter->clipped, (unsigned)meter->count);
}

static esp_err_t capture(bool save, int seconds)
{
	esp_err_t ret = bepio_audio_board_mute(true);
	if (ret != ESP_OK) { return ret; }
	ret = bepio_audio_board_reset_output();
	if (ret != ESP_OK) { return ret; }
	ret = bepio_audio_board_reset_input();
	if (ret != ESP_OK) { return ret; }
	int16_t stereo[BEPIO_AUDIO_TEST_FRAMES * 2];
	/* Discard startup samples; reset_input cleared the old idle RX queue. */
	for (int i = 0; i < 3; i++) {
		if (atomic_load(&stop_requested)) { return ESP_OK; }
		ret = bepio_audio_board_read(stereo, BEPIO_AUDIO_TEST_FRAMES);
		if (ret != ESP_OK) { return ret; }
	}
	if (save) { atomic_store(&recorded_samples, 0); }
	const int slot = atomic_load(&input_slot);
	const size_t wanted = seconds * BEPIO_AUDIO_TEST_RATE;
	bepio_audio_meter_t meters[2] = {0};
	ESP_LOGI(TAG, "%s for %d s; speaker muted; speak near the microphone",
		 save ? "Recording" : "Meter", seconds);
	for (size_t offset = 0; offset < wanted && !atomic_load(&stop_requested);
	     offset += BEPIO_AUDIO_TEST_FRAMES) {
		ret = bepio_audio_board_read(stereo, BEPIO_AUDIO_TEST_FRAMES);
		if (ret != ESP_OK) { break; }
		for (size_t i = 0; i < BEPIO_AUDIO_TEST_FRAMES; i++) {
			bepio_audio_meter_add(&meters[0], stereo[2 * i]);
			bepio_audio_meter_add(&meters[1], stereo[2 * i + 1]);
			if (save) { recording[offset + i] = stereo[2 * i + slot]; }
		}
		if (save) { atomic_store(&recorded_samples, offset + BEPIO_AUDIO_TEST_FRAMES); }
		if (meters[0].count >= BEPIO_AUDIO_TEST_RATE) {
			log_meter("Left ", &meters[0]);
			log_meter("Right", &meters[1]);
			if (!meters[slot].peak) {
				ESP_LOGW(TAG, "Selected input is all zero; check DOUT, input mode and clocks");
			}
			memset(meters, 0, sizeof(meters));
		}
	}
	if (meters[0].count) {
		log_meter("Left ", &meters[0]);
		log_meter("Right", &meters[1]);
	}
	return ret;
}

static esp_err_t drain_output(void)
{
	const int16_t silence[BEPIO_AUDIO_TEST_FRAMES * 2] = {0};
	/* More than a DMA ring of silence lets queued playback reach the DAC. */
	for (int i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT + 2; i++) {
		if (atomic_load(&stop_requested)) { break; }
		esp_err_t ret = bepio_audio_board_write(silence, BEPIO_AUDIO_TEST_FRAMES);
		if (ret != ESP_OK) { return ret; }
	}
	return ESP_OK;
}

static esp_err_t playback(bool tone, int frequency, int seconds)
{
	size_t total = tone ? seconds * BEPIO_AUDIO_TEST_RATE : atomic_load(&recorded_samples);
	if (!total) { return ESP_ERR_INVALID_STATE; }
	esp_err_t ret = bepio_audio_board_reset_output();
	if (ret != ESP_OK) { return ret; }
	ret = bepio_audio_board_mute(false);
	if (ret != ESP_OK) { return ret; }
	int16_t stereo[BEPIO_AUDIO_TEST_FRAMES * 2];
	ESP_LOGI(TAG, "%s: %u samples, volume=%d", tone ? "Test tone" : "Recorded playback",
		 (unsigned)total, atomic_load(&volume));
	for (size_t offset = 0; offset < total && !atomic_load(&stop_requested);) {
		size_t frames = total - offset;
		if (frames > BEPIO_AUDIO_TEST_FRAMES) { frames = BEPIO_AUDIO_TEST_FRAMES; }
		if (tone) {
			bepio_audio_tone(stereo, offset, frames, total, BEPIO_AUDIO_TEST_RATE, frequency);
		} else {
			for (size_t i = 0; i < frames; i++) {
				stereo[2 * i] = stereo[2 * i + 1] = recording[offset + i];
			}
		}
		ret = bepio_audio_board_write(stereo, frames);
		if (ret != ESP_OK) { break; }
		offset += frames;
	}
	if (ret == ESP_OK) { ret = drain_output(); }
	esp_err_t mute_result = bepio_audio_board_mute(true);
	if (ret == ESP_OK) { ret = mute_result; }
	return ret;
}

static void worker(void *arg)
{
	(void)arg;
	audio_request_t request;
	for (;;) {
		xQueueReceive(requests, &request, portMAX_DELAY);
		esp_err_t ret = ESP_OK;
		switch (request.job) {
		case JOB_INIT: ret = initialize(atomic_load(&digital_mic)); break;
		case JOB_MIC: ret = initialize(request.value != 0); break;
		case JOB_TONE: ret = playback(true, request.value, request.seconds); break;
		case JOB_RECORD: ret = capture(true, request.seconds); break;
		case JOB_METER: ret = capture(false, request.seconds); break;
		case JOB_PLAY: ret = playback(false, 0, 0); break;
		case JOB_VOLUME:
			ret = bepio_audio_board_volume(request.value);
			if (ret == ESP_OK) { atomic_store(&volume, request.value); }
			break;
		case JOB_GAIN:
			ret = bepio_audio_board_gain(request.value);
			if (ret == ESP_OK) { atomic_store(&gain_db, request.value); }
			break;
		case JOB_SLOT: atomic_store(&input_slot, request.value); break;
		case JOB_REGS: ret = bepio_audio_board_dump(); break;
		default: ret = ESP_ERR_INVALID_ARG; break;
		}
		if (ret == ESP_OK && request.job == JOB_INIT) {
			ESP_LOGI(TAG, "Automatic test: 1kHz tone for 2s, then microphone levels for 3s");
			ret = playback(true, 1000, 2);
			if (ret == ESP_OK) { ret = capture(false, 3); }
		}
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "%s failed: %s; check wiring, use audio init to retry",
				 job_names[request.job], esp_err_to_name(ret));
		} else {
			ESP_LOGI(TAG, "%s %s", job_names[request.job],
				 atomic_load(&stop_requested) ? "stopped" : "done");
		}
		atomic_store(&last_result, ret);
		atomic_store(&current_job, JOB_IDLE);
	}
}

static int submit(audio_request_t request)
{
	if (!atomic_load(&ready) && request.job != JOB_INIT && request.job != JOB_MIC) {
		puts("Audio unavailable; check power/wiring, then audio init");
		return 1;
	}
	if (request.job == JOB_PLAY && !atomic_load(&recorded_samples)) {
		puts("No recording; run audio record first");
		return 1;
	}
	int expected = JOB_IDLE;
	if (!atomic_compare_exchange_strong(&current_job, &expected, request.job)) {
		puts("Audio busy; use audio stop or wait for completion");
		return 1;
	}
	atomic_store(&stop_requested, false);
	if (xQueueSend(requests, &request, 0) != pdTRUE) {
		atomic_store(&current_job, JOB_IDLE);
		return 1;
	}
	return 0;
}

static bool number(const char *text, int minimum, int maximum, int *value)
{
	char *end;
	errno = 0;
	long result = strtol(text, &end, 10);
	if (errno || end == text || *end || result < minimum || result > maximum) { return false; }
	*value = (int)result;
	return true;
}

static int command(int argc, char **argv)
{
	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "help"))) {
		puts("audio status | init | regs | stop\n"
		     "audio tone [100..4000 Hz] [1..10 seconds] (default 1000 Hz, 2 s)\n"
		     "audio record [1..3 seconds] (default 3 s, replaces RAM recording)\n"
		     "audio play | meter [1..30 seconds] (default 5 s, no replacement)\n"
		     "audio volume <0..100> | gain <0..42 dB, step 6>\n"
		     "audio mic <analog|digital> | slot <left|right>");
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "status")) { status(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "stop")) {
		atomic_store(&stop_requested, true);
		return 0;
	}
	audio_request_t request = {0};
	if (argc == 2 && !strcmp(argv[1], "init")) { request.job = JOB_INIT; }
	else if (argc == 2 && !strcmp(argv[1], "regs")) { request.job = JOB_REGS; }
	else if (argc == 2 && !strcmp(argv[1], "play")) { request.job = JOB_PLAY; }
	else if (argc >= 2 && argc <= 4 && !strcmp(argv[1], "tone")) {
		request = (audio_request_t){JOB_TONE, 1000, 2};
		if (argc >= 3 && !number(argv[2], 100, 4000, &request.value)) { goto invalid; }
		if (argc == 4 && !number(argv[3], 1, 10, &request.seconds)) { goto invalid; }
	} else if (argc >= 2 && argc <= 3 && !strcmp(argv[1], "record")) {
		request = (audio_request_t){.job = JOB_RECORD, .seconds = 3};
		if (argc == 3 && !number(argv[2], 1, RECORD_SECONDS, &request.seconds)) { goto invalid; }
	} else if (argc >= 2 && argc <= 3 && !strcmp(argv[1], "meter")) {
		request = (audio_request_t){.job = JOB_METER, .seconds = 5};
		if (argc == 3 && !number(argv[2], 1, 30, &request.seconds)) { goto invalid; }
	} else if (argc == 3 && !strcmp(argv[1], "volume")) {
		request.job = JOB_VOLUME;
		if (!number(argv[2], 0, 100, &request.value)) { goto invalid; }
	} else if (argc == 3 && !strcmp(argv[1], "gain")) {
		request.job = JOB_GAIN;
		if (!number(argv[2], 0, 42, &request.value) || request.value % 6) { goto invalid; }
	} else if (argc == 3 && !strcmp(argv[1], "mic")) {
		request.job = JOB_MIC;
		if (!strcmp(argv[2], "digital")) { request.value = 1; }
		else if (strcmp(argv[2], "analog")) { goto invalid; }
	} else if (argc == 3 && !strcmp(argv[1], "slot")) {
		request.job = JOB_SLOT;
		if (!strcmp(argv[2], "right")) { request.value = 1; }
		else if (strcmp(argv[2], "left")) { goto invalid; }
	} else { goto invalid; }
	return submit(request);
invalid:
	puts("Invalid command/arguments; use audio help");
	return 1;
}

void app_main(void)
{
	ESP_LOGI(TAG, "ES8311 + NS4150B standalone audio test; output starts muted");
	status();
	requests = xQueueCreate(1, sizeof(audio_request_t));
	ESP_ERROR_CHECK(requests ? ESP_OK : ESP_ERR_NO_MEM);
	ESP_ERROR_CHECK(xTaskCreate(worker, "audio_worker", 6144, NULL, 5, NULL) == pdPASS
		? ESP_OK : ESP_ERR_NO_MEM);
	esp_console_repl_t *repl;
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "audio>";
	config.task_stack_size = 6144;
	config.max_cmdline_length = 256;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
	esp_console_dev_usb_serial_jtag_config_t usb = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb, &config, &repl));
#elif CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
	esp_console_dev_uart_config_t uart = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart, &config, &repl));
#else
#error "t_audio requires a UART or USB Serial/JTAG console"
#endif
	ESP_ERROR_CHECK(esp_console_register_help_command());
	const esp_console_cmd_t cmd = {
		.command = "audio", .help = "Speaker/microphone diagnostics; audio help", .func = command,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	ESP_LOGI(TAG, "Full codec capture: automatic init after 1.5s; analyzer SDA/SCL only");
	vTaskDelay(pdMS_TO_TICKS(1500));
	submit((audio_request_t){.job = JOB_INIT});
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
