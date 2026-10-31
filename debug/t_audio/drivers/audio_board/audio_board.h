#ifndef BEPIO_TEST_AUDIO_BOARD_H_
#define BEPIO_TEST_AUDIO_BOARD_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define BEPIO_AUDIO_TEST_RATE 16000
#define BEPIO_AUDIO_TEST_FRAMES 160
#define BEPIO_AUDIO_TEST_DMA_COUNT 6

/* One worker owns the device. Interleaved L/R, 16-bit Philips I2S. */
esp_err_t bepio_audio_board_init(bool digital_mic, int volume, int gain_db);
void bepio_audio_board_deinit(void);
esp_err_t bepio_audio_board_read(int16_t *stereo, size_t frames);
esp_err_t bepio_audio_board_write(const int16_t *stereo, size_t frames);
esp_err_t bepio_audio_board_reset_input(void);
esp_err_t bepio_audio_board_reset_output(void);
esp_err_t bepio_audio_board_mute(bool mute);
esp_err_t bepio_audio_board_volume(int volume);
esp_err_t bepio_audio_board_gain(int gain_db);
esp_err_t bepio_audio_board_dump(void);
int bepio_audio_board_address(void);

#endif
