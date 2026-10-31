#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bepio_tts.h"
#include "esp_partition.h"
#include "esp_tts.h"

static bool corrupt, cancel_now;
static int resets, destroys, unmapped, parses, stream_index;
static int fail_sink;
static size_t received;
static int64_t now, time_step;
static const int16_t first[] = {1, -2, 3};
static const int16_t second[] = {-4, 5};
static esp_tts_voice_t voice;
static const esp_partition_t partition = {.size = 16};

const esp_partition_t *esp_partition_find_first(int type, int subtype, const char *label)
{
	(void)type; (void)subtype;
	assert(!strcmp(label, "voice_data"));
	return &partition;
}
esp_err_t esp_partition_mmap(const esp_partition_t *p, size_t offset, size_t size,
			   int type, const void **data, esp_partition_mmap_handle_t *mapping)
{
	static const unsigned char bytes[16];
	(void)p; (void)offset; (void)size; (void)type;
	*data = bytes; *mapping = 1;
	return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t mapping)
{
	(void)mapping; unmapped++;
}
int mbedtls_sha256(const unsigned char *data, size_t size, unsigned char output[32], int mode)
{
	(void)data; (void)size; (void)mode;
	memset(output, corrupt ? 1 : 0, 32);
	return 0;
}
int64_t esp_timer_get_time(void)
{
	int64_t result = now;
	now += time_step;
	return result;
}
esp_tts_voice_t *esp_tts_voice_set_init(const esp_tts_voice_t *t, void *bytes)
{
	(void)t; (void)bytes; return &voice;
}
void esp_tts_voice_set_free(esp_tts_voice_t *v) { assert(v == &voice); }
esp_tts_handle_t esp_tts_create(esp_tts_voice_t *v) { return v; }
void esp_tts_destroy(esp_tts_handle_t handle) { assert(handle == &voice); destroys++; }
int esp_tts_parse_chinese(esp_tts_handle_t handle, const char *text)
{
	assert(handle == &voice); parses++; stream_index = 0;
	return strcmp(text, "unsupported") != 0;
}
short *esp_tts_stream_play(esp_tts_handle_t handle, int *count, unsigned speed)
{
	assert(handle == &voice && speed <= 5);
	if (stream_index++ == 0) { *count = 3; return (short *)first; }
	if (stream_index == 2) { *count = 2; return (short *)second; }
	*count = 0; return NULL;
}
void esp_tts_stream_reset(esp_tts_handle_t handle) { assert(handle == &voice); resets++; }
static esp_err_t sink(const int16_t *pcm, size_t count, void *context)
{
	(void)context;
	/* The gate must reject deinit/reentry while a stream callback is running. */
	assert(bepio_tts_deinit() == ESP_ERR_INVALID_STATE);
	if (fail_sink) { return ESP_FAIL; }
	if (!received) { assert(count == 3 && !memcmp(pcm, first, sizeof(first))); }
	else { assert(count == 2 && !memcmp(pcm, second, sizeof(second))); }
	received += count;
	return ESP_OK;
}
static bool cancel(void *context) { (void)context; return cancel_now; }

int main(void)
{
	assert(!bepio_tts_text_valid(NULL));
	assert(!bepio_tts_text_valid(" \t\n"));
	assert(bepio_tts_text_valid("你好，今天下午三点回家。"));
	assert(!bepio_tts_text_valid("\xc0\x80"));
	assert(!bepio_tts_text_valid("\xed\xa0\x80"));
	assert(!bepio_tts_text_valid("\xf4\x90\x80\x80"));
	assert(!bepio_tts_text_valid("\xe4\xbd"));
	char text[258];
	memset(text, 'a', 256); text[256] = 0;
	assert(bepio_tts_text_valid(text));
	text[256] = 'a'; text[257] = 0;
	assert(!bepio_tts_text_valid(text));
	const bepio_tts_output_t output = {.write = sink, .cancelled = cancel};
	size_t samples;
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_ERR_INVALID_STATE);
	corrupt = true;
	assert(bepio_tts_init() == ESP_ERR_INVALID_CRC && !bepio_tts_ready());
	assert(unmapped == 1 && parses == 0);
	corrupt = false;
	assert(bepio_tts_init() == ESP_OK && bepio_tts_ready());
	assert(bepio_tts_init() == ESP_OK); /* Idempotent, no second mapping. */
	assert(bepio_tts_speak("你好", 6, &output, &samples) == ESP_ERR_INVALID_ARG);
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_OK);
	assert(samples == 5 && received == 5 && resets == 1);
	cancel_now = true;
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_ERR_NOT_FINISHED);
	assert(samples == 0 && parses == 1 && resets == 2);
	cancel_now = false;
	fail_sink = 1;
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_FAIL);
	assert(samples == 0 && resets == 3);
	fail_sink = 0;
	received = 0;
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_OK);
	assert(samples == 5 && received == 5 && resets == 4);
	assert(bepio_tts_speak("unsupported", 3, &output, &samples) == ESP_ERR_NOT_SUPPORTED);
	assert(resets == 5);
	time_step = 60000000;
	assert(bepio_tts_speak("你好", 3, &output, &samples) == ESP_ERR_TIMEOUT);
	assert(samples == 0 && resets == 6);
	assert(bepio_tts_deinit() == ESP_OK && !bepio_tts_ready());
	assert(destroys == 1 && unmapped == 2);
	puts("PASS: UTF-8/bounds, voice verification, PCM streaming, cancellation, sink errors, reset and ownership");
	return 0;
}
