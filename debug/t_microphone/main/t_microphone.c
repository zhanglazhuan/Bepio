#include <math.h>
#include <stdio.h>
#include <string.h>
#include "audio_board.h"
#include "audio_signal.h"
#include "bepio_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RECORD_SECONDS 3
#define RATE BEPIO_AUDIO_TEST_RATE
#define FRAMES BEPIO_AUDIO_TEST_FRAMES
/* 192 KB internal RAM, reused between modes. No PSRAM or persistent recording. */
static int16_t recording[RATE * RECORD_SECONDS * 2];
static const char *TAG = "t_microphone";
static bool captured;

typedef struct {
	bepio_audio_meter_t meter;
	size_t nonzero;
	int minimum, maximum;
} channel_stats_t;

static void add(channel_stats_t *stats, int16_t value)
{
	if (!stats->meter.count || value < stats->minimum) { stats->minimum = value; }
	if (!stats->meter.count || value > stats->maximum) { stats->maximum = value; }
	stats->nonzero += value != 0;
	bepio_audio_meter_add(&stats->meter, value);
}

static void log_stats(const char *mode, const char *phase, int second, int slot, const channel_stats_t *s)
{
	ESP_LOGI(TAG, "%s %s sec=%d %s RMS=%.1f dBFS=%.1f peak=%d min=%d max=%d nonzero=%u/%u clipped=%u",
		mode, phase, second, slot ? "Right" : "Left",
		bepio_audio_meter_rms(&s->meter), bepio_audio_meter_dbfs(&s->meter), s->meter.peak,
		s->minimum, s->maximum, (unsigned)s->nonzero, (unsigned)s->meter.count, (unsigned)s->meter.clipped);
}

static esp_err_t capture(const char *mode, const char *phase, unsigned seconds,
			 bool save, channel_stats_t totals[2])
{
	if (!seconds || (save && seconds > RECORD_SECONDS)) { return ESP_ERR_INVALID_ARG; }
	esp_err_t ret = bepio_audio_board_mute(true);
	if (ret == ESP_OK) { ret = bepio_audio_board_reset_output(); }
	if (ret == ESP_OK) { ret = bepio_audio_board_reset_input(); }
	if (ret != ESP_OK) { return ret; }
	int16_t stereo[FRAMES * 2];
	/* Discard stale DMA and codec startup samples before each observation. */
	for (unsigned i = 0; i < (save ? 10U : 100U); i++) {
		ret = bepio_audio_board_read(stereo, FRAMES);
		if (ret != ESP_OK) { return ret; }
	}
	memset(totals, 0, sizeof(channel_stats_t) * 2);
	channel_stats_t window[2] = {0};
	const size_t wanted = seconds * RATE;
	for (size_t offset = 0; offset < wanted; offset += FRAMES) {
		ret = bepio_audio_board_read(stereo, FRAMES);
		if (ret != ESP_OK) { return ret; }
		if (save) { memcpy(recording + offset * 2, stereo, sizeof(stereo)); }
		for (unsigned i = 0; i < FRAMES; i++) {
			for (unsigned slot = 0; slot < 2; slot++) {
				add(&window[slot], stereo[2 * i + slot]);
				add(&totals[slot], stereo[2 * i + slot]);
			}
		}
		if ((offset + FRAMES) % RATE == 0) {
			for (unsigned slot = 0; slot < 2; slot++) {
				log_stats(mode, phase, (offset + FRAMES) / RATE, slot, &window[slot]);
			}
			memset(window, 0, sizeof(window));
		}
	}
	return ESP_OK;
}

static esp_err_t playback(unsigned slot, const channel_stats_t *stats)
{
	esp_err_t ret = bepio_audio_board_reset_output();
	if (ret == ESP_OK) { ret = bepio_audio_board_mute(false); }
	if (ret != ESP_OK) { return ret; }
	int16_t stereo[FRAMES * 2];
	double dc = (double)stats->meter.sum / stats->meter.count;
	double rms = bepio_audio_meter_rms(&stats->meter);
	double extent = fmax(fabs(stats->minimum - dc), fabs(stats->maximum - dc));
	/* Diagnostic listening gain, not a claim that captured samples are voice. */
	double scale = fmin(32.0, fmin(1500.0 / fmax(rms, 1.0), 24000.0 / fmax(extent, 1.0)));
	ESP_LOGI(TAG, "Replay %s for 3s; DC=%.1f listening gain=%.1fx (max 32x)", slot ? "Right" : "Left", dc, scale);
	for (unsigned offset = 0; offset < RATE * RECORD_SECONDS; offset += FRAMES) {
		for (unsigned i = 0; i < FRAMES; i++) {
			double value = (recording[(offset + i) * 2 + slot] - dc) * scale;
			int16_t pcm = (int16_t)fmax(-30000.0, fmin(30000.0, value));
			stereo[2 * i] = stereo[2 * i + 1] = pcm;
		}
		ret = bepio_audio_board_write(stereo, FRAMES);
		if (ret != ESP_OK) { break; }
	}
	if (ret == ESP_OK) {
		memset(stereo, 0, sizeof(stereo));
		for (unsigned i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT + 2; i++) {
			ret = bepio_audio_board_write(stereo, FRAMES);
			if (ret != ESP_OK) { break; }
		}
	}
	esp_err_t mute_result = bepio_audio_board_mute(true);
	return ret == ESP_OK ? mute_result : ret;
}

static esp_err_t cue(unsigned frequency)
{
	esp_err_t ret = bepio_audio_board_reset_output();
	if (ret == ESP_OK) { ret = bepio_audio_board_mute(false); }
	if (ret != ESP_OK) { return ret; }
	int16_t samples[FRAMES * 2];
	const unsigned total = RATE / 5;
	for (unsigned offset = 0; offset < total; offset += FRAMES) {
		bepio_audio_tone(samples, offset, FRAMES, total, RATE, frequency);
		ret = bepio_audio_board_write(samples, FRAMES);
		if (ret != ESP_OK) { break; }
	}
	if (ret == ESP_OK) {
		memset(samples, 0, sizeof(samples));
		for (unsigned i = 0; i < BEPIO_AUDIO_TEST_DMA_COUNT + 2; i++) {
			ret = bepio_audio_board_write(samples, FRAMES);
			if (ret != ESP_OK) { break; }
		}
	}
	esp_err_t muted = bepio_audio_board_mute(true);
	return ret == ESP_OK ? muted : ret;
}

static esp_err_t test_mode(bool digital)
{
	captured = false;
	const char *mode = digital ? "DIGITAL" : "ANALOG";
	ESP_LOGI(TAG, "=== %s microphone test BEGIN ===", mode);
	esp_err_t ret = bepio_audio_board_init(digital, 60, 24);
	if (ret != ESP_OK) { return ret; }
	channel_stats_t quiet[2], speech[2];
	ESP_LOGI(TAG, "%s: keep quiet during settling, then 1s baseline; speaker muted", mode);
	ret = capture(mode, "QUIET", 1, false, quiet);
	if (ret != ESP_OK) { return ret; }
	for (int count = 3; count > 0; count--) {
		ESP_LOGI(TAG, "%s: recording countdown %d", mode, count);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	ret = cue(1000);
	if (ret != ESP_OK) { return ret; }
	ESP_LOGI(TAG, ">>> %s RECORDING NOW: speak for 3 seconds <<<", mode);
	ret = capture(mode, "SPEECH", RECORD_SECONDS, true, speech);
	if (ret != ESP_OK) { return ret; }
	captured = true;
	ESP_LOGI(TAG, ">>> RECORDING FINISHED: stop speaking <<<");
	ret = cue(500);
	if (ret != ESP_OK) { return ret; }
	double left = bepio_audio_meter_rms(&speech[0].meter);
	double right = bepio_audio_meter_rms(&speech[1].meter);
	unsigned slot = right > left ? 1 : 0;
	double rms = slot ? right : left;
	double baseline = bepio_audio_meter_rms(&quiet[slot].meter);
	ESP_LOGI(TAG, "%s result: selected=%s quiet_RMS=%.1f speech_RMS=%.1f change=%.1fdB",
		mode, slot ? "Right" : "Left", baseline, rms,
		20.0 * log10(fmax(rms, 0.001) / fmax(baseline, 0.001)));
	if (rms < 4.0) {
		ESP_LOGW(TAG, "%s input near silence or DC only; skip replay. This mode has no confirmed microphone signal", mode);
		return bepio_audio_board_dump();
	}
	if (rms < baseline * 2.0) {
		ESP_LOGW(TAG, "%s no clear speech-level increase; amplified replay may only contain background noise", mode);
	}
	ESP_LOGI(TAG, "%s contains AC samples; amplified replay does not prove voice capture", mode);
	return playback(slot, &speech[slot]);
}

void app_main(void)
{
	ESP_LOGI(TAG, "Automatic microphone A/B test; see audio_board log for I2C backend; no commands");
	ESP_LOGI(TAG, "SDA=%d SCL=%d MCLK=%d BCLK=%d LRCK=%d RX=%d TX=%d; digital then analog",
		BEPIO_AUDIO_SDA_GPIO, BEPIO_AUDIO_SCL_GPIO, BEPIO_AUDIO_MCLK_GPIO,
		BEPIO_AUDIO_BCLK_GPIO, BEPIO_AUDIO_LRCK_GPIO, BEPIO_AUDIO_RX_GPIO, BEPIO_AUDIO_TX_GPIO);
	vTaskDelay(pdMS_TO_TICKS(1500));
	unsigned captures = 0;
	for (unsigned mode = 0; mode < 2; mode++) {
		esp_err_t ret = test_mode(mode == 0);
		captures += captured;
		if (!captured) {
			ESP_LOGW(TAG, "%s: NO COMPLETE RECORDING; result=%s; microphone response not evaluated",
				mode == 0 ? "DIGITAL" : "ANALOG", esp_err_to_name(ret));
		}
		ESP_LOGI(TAG, "=== %s microphone test END: %s ===", mode == 0 ? "DIGITAL" : "ANALOG", esp_err_to_name(ret));
		bepio_audio_board_deinit();
		memset(recording, 0, sizeof(recording));
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	ESP_LOGI(TAG, "Test run ended: %u/2 modes captured complete PCM. This does not confirm voice; RESET to repeat", captures);
}
