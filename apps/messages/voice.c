#include "voice.h"
#include "audio_board.h"
#include "bepio_tts.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

static QueueHandle_t requests;
static const char *TAG = "messages_voice";

static esp_err_t write_pcm(const int16_t *mono, size_t samples, void *context)
{
	(void)context;
	int16_t stereo[BEPIO_AUDIO_TEST_FRAMES * 2];
	for (size_t offset = 0; offset < samples;) {
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

static void worker(void *context)
{
	(void)context;
	char text[BEPIO_TTS_TEXT_MAX + 1];
	bool ready = false;
	for (;;) {
		xQueueReceive(requests, text, portMAX_DELAY);
		if (!ready) {
			esp_err_t ret = bepio_tts_ready() ? ESP_OK : bepio_tts_init();
			if (ret == ESP_OK) { ret = bepio_audio_board_init(false, 60, 24); }
			if (ret != ESP_OK) { ESP_LOGE(TAG, "Audio init failed: %s", esp_err_to_name(ret)); continue; }
			ready = true;
		}
		esp_err_t ret = bepio_audio_board_reset_output();
		if (ret == ESP_OK) { ret = bepio_audio_board_mute(false); }
		if (ret == ESP_OK) {
			const bepio_tts_output_t output = {.write = write_pcm};
			ret = bepio_tts_speak(text, 3, &output, NULL);
			if (ret == ESP_OK) {
				const int16_t silence[BEPIO_AUDIO_TEST_FRAMES] = {0};
				for (int i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT + 2; i++) {
					ret = write_pcm(silence, BEPIO_AUDIO_TEST_FRAMES, NULL);
					if (ret != ESP_OK) { break; }
				}
			}
		}
		(void)bepio_audio_board_mute(true);
		if (ret != ESP_OK) { ESP_LOGW(TAG, "TTS playback failed: %s", esp_err_to_name(ret)); }
	}
}

int bepio_messages_voice_play(const char *text)
{
	if (!bepio_tts_text_valid(text)) { return -EINVAL; }
	bool chinese = false;
	for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
		if (*p >= 0xe4 && *p <= 0xe9) { chinese = true; break; }
	}
	if (!chinese) { return -ENOTSUP; }
	if (!requests) {
		requests = xQueueCreate(1, BEPIO_TTS_TEXT_MAX + 1);
		if (!requests) { return -ENOMEM; }
		if (xTaskCreate(worker, "messages_voice", 12288, NULL, 5, NULL) != pdPASS) {
			vQueueDelete(requests); requests = NULL; return -ENOMEM;
		}
	}
	char request[BEPIO_TTS_TEXT_MAX + 1] = {0};
	strcpy(request, text);
	return xQueueSend(requests, request, 0) == pdPASS ? 0 : -EBUSY;
}
