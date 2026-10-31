#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_board.h"
#include "bepio_tts.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

_Static_assert(BEPIO_TTS_SAMPLE_RATE == BEPIO_AUDIO_TEST_RATE, "TTS/audio sample rate mismatch");

typedef enum { INIT, SAY, VOLUME, SPEED } job_t;
typedef struct {
	job_t job;
	int value;
	char text[BEPIO_TTS_TEXT_MAX + 1];
} request_t;

static const char *TAG = "t_tts";
static QueueHandle_t requests;
static atomic_bool busy, cancelled, audio_ready;
static atomic_int last_result, volume = 60, speed = 3;
static atomic_uint last_samples, last_elapsed_ms;

static bool stopping(void *context)
{
	(void)context;
	return atomic_load(&cancelled);
}

static esp_err_t write_pcm(const int16_t *mono, size_t samples, void *context)
{
	(void)context;
	int16_t stereo[BEPIO_AUDIO_TEST_FRAMES * 2];
	for (size_t offset = 0; offset < samples;) {
		if (stopping(NULL)) { return ESP_ERR_NOT_FINISHED; }
		size_t frames = samples - offset;
		if (frames > BEPIO_AUDIO_TEST_FRAMES) { frames = BEPIO_AUDIO_TEST_FRAMES; }
		for (size_t i = 0; i < frames; i++) {
			stereo[2 * i] = stereo[2 * i + 1] = mono[offset + i];
		}
		esp_err_t ret = bepio_audio_board_write(stereo, frames);
		if (ret != ESP_OK) { return ret; }
		offset += frames;
	}
	return ESP_OK;
}

static esp_err_t initialize(void)
{
	esp_err_t ret = bepio_tts_init();
	if (ret != ESP_OK) { return ret; }
	atomic_store(&audio_ready, false);
	ret = bepio_audio_board_init(false, atomic_load(&volume), 24);
	atomic_store(&audio_ready, ret == ESP_OK);
	return ret;
}

static esp_err_t speak(const char *text)
{
	atomic_store(&last_samples, 0);
	atomic_store(&last_elapsed_ms, 0);
	esp_err_t ret = bepio_audio_board_reset_output();
	if (ret != ESP_OK) { return ret; }
	ret = bepio_audio_board_mute(false);
	if (ret != ESP_OK) { return ret; }
	const bepio_tts_output_t output = {.write = write_pcm, .cancelled = stopping};
	size_t samples = 0;
	int64_t start = esp_timer_get_time();
	ret = bepio_tts_speak(text, atomic_load(&speed), &output, &samples);
	if (ret == ESP_OK) {
		const int16_t silence[BEPIO_AUDIO_TEST_FRAMES] = {0};
		for (int i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT + 2; i++) {
			ret = write_pcm(silence, BEPIO_AUDIO_TEST_FRAMES, NULL);
			if (ret != ESP_OK) { break; }
		}
	}
	esp_err_t mute_result = bepio_audio_board_mute(true);
	if (ret == ESP_OK) { ret = mute_result; }
	atomic_store(&last_samples, samples);
	atomic_store(&last_elapsed_ms, (unsigned)((esp_timer_get_time() - start) / 1000));
	return ret;
}

static void status(void)
{
	unsigned count = atomic_load(&last_samples);
	printf("voice_ready=%d audio_ready=%d busy=%d speed=%d volume=%d last=%s\n",
		bepio_tts_ready(), atomic_load(&audio_ready), atomic_load(&busy),
		atomic_load(&speed), atomic_load(&volume), esp_err_to_name(atomic_load(&last_result)));
	printf("Last: %u samples / %u ms audio; elapsed=%u ms (includes playback)\n",
		count, count * 1000 / BEPIO_TTS_SAMPLE_RATE, atomic_load(&last_elapsed_ms));
	printf("Internal heap free=%u largest=%u bytes; 16kHz/16bit mono synthesis\n",
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
		(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static void worker(void *arg)
{
	(void)arg;
	request_t request;
	for (;;) {
		xQueueReceive(requests, &request, portMAX_DELAY);
		esp_err_t ret;
		switch (request.job) {
		case INIT: ret = initialize(); break;
		case SAY: ret = speak(request.text); break;
		case VOLUME:
			ret = bepio_audio_board_volume(request.value);
			if (ret == ESP_OK) { atomic_store(&volume, request.value); }
			break;
		case SPEED: atomic_store(&speed, request.value); ret = ESP_OK; break;
		default: ret = ESP_ERR_INVALID_ARG; break;
		}
		atomic_store(&last_result, ret);
		if (ret == ESP_ERR_NOT_FINISHED) { ESP_LOGI(TAG, "Playback stopped"); }
		else if (ret != ESP_OK) { ESP_LOGE(TAG, "Failed: %s; use tts status / tts init", esp_err_to_name(ret)); }
		else { ESP_LOGI(TAG, "Done"); }
		atomic_store(&busy, false);
	}
}

static int submit(const request_t *request)
{
	if (request->job != INIT && request->job != SPEED &&
	    (!bepio_tts_ready() || !atomic_load(&audio_ready))) {
		puts("TTS/audio unavailable; run full idf.py flash, check wiring, then tts init");
		return 1;
	}
	bool expected = false;
	if (!atomic_compare_exchange_strong(&busy, &expected, true)) {
		puts("Busy; use tts stop or wait for Done");
		return 1;
	}
	atomic_store(&cancelled, false);
	if (xQueueSend(requests, request, 0) != pdTRUE) {
		atomic_store(&busy, false);
		return 1;
	}
	return 0;
}

static bool number(const char *text, int maximum, int *value)
{
	char *end;
	errno = 0;
	long n = strtol(text, &end, 10);
	if (errno || end == text || *end || n < 0 || n > maximum) { return false; }
	*value = (int)n;
	return true;
}

static int command(int argc, char **argv)
{
	if (argc == 1 || (argc == 2 && !strcmp(argv[1], "help"))) {
		puts("tts init | demo | status | stop\n"
		     "tts say \"UTF-8 Chinese text, max 256 bytes\"\n"
		     "tts speed <0..5, slow..fast> | volume <0..100>");
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "status")) { status(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "stop")) {
		atomic_store(&cancelled, true);
		return 0;
	}
	request_t request = {0};
	if (argc == 2 && !strcmp(argv[1], "init")) { request.job = INIT; }
	else if (argc == 2 && !strcmp(argv[1], "demo")) {
		request.job = SAY;
		strcpy(request.text, "你好，我是小班。已经收到新消息，请注意查看。");
	} else if (argc == 3 && !strcmp(argv[1], "say")) {
		if (!bepio_tts_text_valid(argv[2])) {
			puts("Text must be valid UTF-8, nonempty, at most 256 bytes");
			return 1;
		}
		request.job = SAY;
		strcpy(request.text, argv[2]);
	} else if (argc == 3 && !strcmp(argv[1], "speed")) {
		request.job = SPEED;
		if (!number(argv[2], 5, &request.value)) { goto invalid; }
	} else if (argc == 3 && !strcmp(argv[1], "volume")) {
		request.job = VOLUME;
		if (!number(argv[2], 100, &request.value)) { goto invalid; }
	} else { goto invalid; }
	return submit(&request);
invalid:
	puts("Invalid arguments; use tts help");
	return 1;
}

void app_main(void)
{
	ESP_LOGI(TAG, "ESP-TTS Chinese playback; MCLK wiring is inherited from bepio_audio.h");
	requests = xQueueCreate(1, sizeof(request_t));
	ESP_ERROR_CHECK(requests ? ESP_OK : ESP_ERR_NO_MEM);
	ESP_ERROR_CHECK(xTaskCreate(worker, "tts_worker", 12288, NULL, 5, NULL) == pdPASS
		? ESP_OK : ESP_ERR_NO_MEM);
	esp_console_repl_t *repl;
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "tts>";
	config.task_stack_size = 6144;
	config.max_cmdline_length = 384;
	esp_console_dev_usb_serial_jtag_config_t usb = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb, &config, &repl));
	ESP_ERROR_CHECK(esp_console_register_help_command());
	const esp_console_cmd_t cmd = {
		.command = "tts", .help = "Offline Chinese speech; tts help", .func = command,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	request_t init = {.job = INIT};
	submit(&init);
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
