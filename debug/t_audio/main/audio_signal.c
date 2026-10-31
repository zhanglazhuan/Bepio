#include "audio_signal.h"
#include <math.h>

void bepio_audio_meter_add(bepio_audio_meter_t *meter, int16_t sample)
{
	int value = sample;
	int magnitude = value < 0 ? -value : value;
	meter->sum += value;
	meter->square += (int64_t)value * value;
	meter->count++;
	meter->clipped += magnitude >= 32760;
	if (magnitude > meter->peak) { meter->peak = magnitude; }
}

double bepio_audio_meter_rms(const bepio_audio_meter_t *meter)
{
	if (!meter->count) { return 0; }
	double mean = (double)meter->sum / meter->count;
	return sqrt(fmax(0, (double)meter->square / meter->count - mean * mean));
}

double bepio_audio_meter_dbfs(const bepio_audio_meter_t *meter)
{
	return 20 * log10(fmax(bepio_audio_meter_rms(meter), 0.001) / 32768.0);
}

void bepio_audio_tone(int16_t *stereo, size_t offset, size_t frames,
		     size_t total_frames, unsigned rate, unsigned frequency)
{
	size_t ramp = rate / 100; /* 10 ms fade at each end. */
	for (size_t i = 0; i < frames; i++) {
		size_t position = offset + i;
		double envelope = fmin(1.0, fmin((double)position / ramp,
			(double)(total_frames - 1 - position) / ramp));
		int16_t value = (int16_t)(6000 * envelope *
			sin(6.283185307179586 * frequency * position / rate));
		stereo[2 * i] = stereo[2 * i + 1] = value;
	}
}
