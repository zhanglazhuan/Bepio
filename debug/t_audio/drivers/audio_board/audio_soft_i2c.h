#ifndef BEPIO_AUDIO_SOFT_I2C_H_
#define BEPIO_AUDIO_SOFT_I2C_H_
#include "esp_codec_dev_defaults.h"
esp_err_t bepio_audio_soft_i2c_init(void);
void bepio_audio_soft_i2c_deinit(void);
esp_err_t bepio_audio_soft_i2c_probe(uint8_t address);
const audio_codec_ctrl_if_t *bepio_audio_soft_i2c_ctrl(uint8_t address);
#endif
