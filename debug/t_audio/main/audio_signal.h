#ifndef BEPIO_TEST_AUDIO_SIGNAL_H_
#define BEPIO_TEST_AUDIO_SIGNAL_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
	int64_t sum;
	uint64_t square;
	size_t count;
	size_t clipped;
	int peak;
} bepio_audio_meter_t;

void bepio_audio_meter_add(bepio_audio_meter_t *meter, int16_t sample);
double bepio_audio_meter_rms(const bepio_audio_meter_t *meter);
double bepio_audio_meter_dbfs(const bepio_audio_meter_t *meter);
void bepio_audio_tone(int16_t *stereo, size_t offset, size_t frames,
		     size_t total_frames, unsigned rate, unsigned frequency);

#endif
