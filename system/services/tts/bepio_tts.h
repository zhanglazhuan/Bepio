#ifndef BEPIO_TTS_H_
#define BEPIO_TTS_H_
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "tts_text.h"

#define BEPIO_TTS_SAMPLE_RATE 16000

typedef struct {
	/* Mono signed 16-bit PCM; consume synchronously, do not free or retain it. */
	esp_err_t (*write)(const int16_t *pcm, size_t samples, void *context);
	bool (*cancelled)(void *context);
	void *context;
} bepio_tts_output_t;

/* The service is non-reentrant. Concurrent init/speak/deinit returns invalid state.
 * speak blocks; call on an audio task, not on the modem's UART receive task.
 */
esp_err_t bepio_tts_init(void);
esp_err_t bepio_tts_deinit(void);
bool bepio_tts_ready(void);
esp_err_t bepio_tts_speak(const char *text, unsigned speed,
			 const bepio_tts_output_t *output, size_t *samples_written);

#endif
