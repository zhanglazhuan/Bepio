#include "bepio_tts.h"
#include <stdatomic.h>
#include <string.h>
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_tts.h"
#include "esp_tts_voice_template.h"
#include "mbedtls/sha256.h"
#include "voice_model.h"

static const char *TAG = "bepio_tts";
static atomic_flag busy = ATOMIC_FLAG_INIT;
static atomic_bool initialized;
static esp_partition_mmap_handle_t mapping;
static bool mapped;
static esp_tts_voice_t *voice;
static esp_tts_handle_t handle;

static void release(void)
{
	atomic_store(&initialized, false);
	if (handle) { esp_tts_destroy(handle); handle = NULL; }
	if (voice) { esp_tts_voice_set_free(voice); voice = NULL; }
	if (mapped) { esp_partition_munmap(mapping); mapped = false; }
}

esp_err_t bepio_tts_init(void)
{
	if (atomic_flag_test_and_set(&busy)) { return ESP_ERR_INVALID_STATE; }
	esp_err_t ret = ESP_OK;
	if (atomic_load(&initialized)) { goto done; }
	const esp_partition_t *part = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "voice_data");
	if (!part || part->size < BEPIO_TTS_VOICE_BYTES) {
		ret = ESP_ERR_NOT_FOUND;
		ESP_LOGE(TAG, "voice_data partition missing or too small");
		goto done;
	}
	const void *bytes;
	ret = esp_partition_mmap(part, 0, BEPIO_TTS_VOICE_BYTES,
		ESP_PARTITION_MMAP_DATA, &bytes, &mapping);
	if (ret != ESP_OK) { goto done; }
	mapped = true;
	/* Never hand erased/corrupt Flash to the opaque voice-data parser. */
	uint8_t digest[32];
	if (mbedtls_sha256(bytes, BEPIO_TTS_VOICE_BYTES, digest, 0) != 0 ||
	    memcmp(digest, bepio_tts_voice_sha256, sizeof(digest))) {
		ESP_LOGE(TAG, "Voice SHA256 mismatch; run idf.py flash, not app-flash");
		ret = ESP_ERR_INVALID_CRC;
		goto done;
	}
	voice = esp_tts_voice_set_init(&esp_tts_voice_template, (void *)bytes);
	if (!voice) { ret = ESP_ERR_NO_MEM; goto done; }
	handle = esp_tts_create(voice);
	if (!handle) { ret = ESP_ERR_NO_MEM; goto done; }
	atomic_store(&initialized, true);
	ESP_LOGI(TAG, "Xiaoxin Chinese voice verified: %u bytes, UTF-8 -> 16kHz/16bit mono",
		 (unsigned)BEPIO_TTS_VOICE_BYTES);
done:
	if (ret != ESP_OK) { release(); }
	atomic_flag_clear(&busy);
	return ret;
}

esp_err_t bepio_tts_deinit(void)
{
	if (atomic_flag_test_and_set(&busy)) { return ESP_ERR_INVALID_STATE; }
	release();
	atomic_flag_clear(&busy);
	return ESP_OK;
}

bool bepio_tts_ready(void)
{
	return atomic_load(&initialized);
}

esp_err_t bepio_tts_speak(const char *text, unsigned speed,
			 const bepio_tts_output_t *output, size_t *samples_written)
{
	if (samples_written) { *samples_written = 0; }
	if (!bepio_tts_text_valid(text) || speed > 5 || !output || !output->write) {
		return ESP_ERR_INVALID_ARG;
	}
	if (atomic_flag_test_and_set(&busy)) { return ESP_ERR_INVALID_STATE; }
	esp_err_t ret = ESP_OK;
	size_t samples = 0;
	if (!atomic_load(&initialized)) { ret = ESP_ERR_INVALID_STATE; goto done; }
	if (output->cancelled && output->cancelled(output->context)) {
		ret = ESP_ERR_NOT_FINISHED;
		goto done;
	}
	if (!esp_tts_parse_chinese(handle, text)) { ret = ESP_ERR_NOT_SUPPORTED; goto done; }
	int64_t deadline = esp_timer_get_time() + 60000000;
	for (;;) {
		if (output->cancelled && output->cancelled(output->context)) {
			ret = ESP_ERR_NOT_FINISHED;
			break;
		}
		if (esp_timer_get_time() >= deadline) { ret = ESP_ERR_TIMEOUT; break; }
		int count = 0;
		short *pcm = esp_tts_stream_play(handle, &count, speed);
		if (!count) { break; }
		if (count < 0 || !pcm) { ret = ESP_FAIL; break; }
		ret = output->write((const int16_t *)pcm, count, output->context);
		if (ret != ESP_OK) { break; }
		samples += count;
	}
	if (ret == ESP_OK && !samples) { ret = ESP_ERR_NOT_SUPPORTED; }
done:
	if (handle) { esp_tts_stream_reset(handle); }
	if (samples_written) { *samples_written = samples; }
	atomic_flag_clear(&busy);
	return ret;
}
