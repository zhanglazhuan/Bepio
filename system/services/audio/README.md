# audio

音频板驱动已提供采集和播放接口。Messages 已接入中文文本的按需 TTS 播放；自由口述转写与按住录音发送尚未接入主固件。
ES8311 + NS4150B 子板接线已迁移到 `boards/esp32s3/bepio_audio.h`。
驱动使用 ESP-IDF 的 I2C/I2S，不使用 Zephyr shield。
主固件默认用软件 I2C 配置 ES8311；同一块音频板已在独立音频测试中通过软件 I2C 初始化并播放测试音。需要对照原生 I2C 时，可在主工程配置 `-D BEPIO_AUDIO_HARDWARE_I2C=ON` 构建。主固件的实际播放仍需实板验证。

独立硬件验证见 [`debug/t_audio`](../../../debug/t_audio/README.md)。

中文合成接口位于 [`../tts`](../tts/README.md)，独立 [`debug/t_tts`](../../../debug/t_tts/README.md) 复用音频测试驱动验证文本播报。
