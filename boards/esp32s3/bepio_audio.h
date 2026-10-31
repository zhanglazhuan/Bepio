#ifndef BEPIO_AUDIO_H_
#define BEPIO_AUDIO_H_
/* ES8311 + NS4150B: data pin names are from the codec's perspective. */
#define BEPIO_AUDIO_I2C_PORT 0
#define BEPIO_AUDIO_SDA_GPIO 41
#define BEPIO_AUDIO_SCL_GPIO 42
#define BEPIO_AUDIO_I2C_FREQ_HZ 100000
#define BEPIO_AUDIO_CODEC_ADDR 0x18 /* CE=0 assumed; verify 0x18 vs 0x19. */
#define BEPIO_AUDIO_I2S_PORT 0
#define BEPIO_AUDIO_MCLK_GPIO 7
#define BEPIO_AUDIO_BCLK_GPIO 4
#define BEPIO_AUDIO_LRCK_GPIO 15
#define BEPIO_AUDIO_RX_GPIO 6 /* Codec DOUT -> ESP32-S3 RX. */
#define BEPIO_AUDIO_TX_GPIO 16 /* ESP32-S3 TX -> codec DIN. */
#endif
