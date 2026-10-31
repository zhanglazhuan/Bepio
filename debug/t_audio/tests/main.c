#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "audio_signal.h"

int main(void)
{
	bepio_audio_meter_t meter = {0};
	assert(bepio_audio_meter_rms(&meter) == 0);
	for (int i = 0; i < 100; i++) { bepio_audio_meter_add(&meter, 1234); }
	assert(meter.peak == 1234 && meter.clipped == 0);
	assert(bepio_audio_meter_rms(&meter) == 0); /* DC must not appear as sound. */
	meter = (bepio_audio_meter_t){0};
	bepio_audio_meter_add(&meter, INT16_MIN);
	bepio_audio_meter_add(&meter, INT16_MAX);
	assert(meter.peak == 32768 && meter.clipped == 2);
	assert(fabs(bepio_audio_meter_rms(&meter) - 32767.5) < 0.01);
	assert(fabs(bepio_audio_meter_dbfs(&meter)) < 0.001);

	int16_t block[320];
	meter = (bepio_audio_meter_t){0};
	int crossings = 0;
	int16_t previous = 0;
	for (size_t offset = 0; offset < 16000; offset += 160) {
		bepio_audio_tone(block, offset, 160, 16000, 16000, 1000);
		if (!offset) { assert(block[0] == 0); }
		for (size_t i = 0; i < 160; i++) {
			assert(block[2 * i] == block[2 * i + 1]);
			if (previous <= 0 && block[2 * i] > 0) { crossings++; }
			previous = block[2 * i];
			bepio_audio_meter_add(&meter, previous);
		}
	}
	assert(previous == 0); /* Fade reaches silence. */
	assert(crossings >= 999 && crossings <= 1001); /* Actual 1 kHz, no block phase reset. */
	assert(meter.peak <= 6000 && meter.peak > 5900 && meter.clipped == 0);
	assert(bepio_audio_meter_rms(&meter) > 4150 && bepio_audio_meter_rms(&meter) < 4250);
	puts("PASS: stereo tone frequency/fades, DC rejection, RMS, full-scale peaks and clipping");
	return 0;
}
