# 独立调试工程

当前主工程已迁移 ESP-IDF v5.5.3，见根目录 README.md。

- [`t_tts/`](t_tts/README.md)：ESP-TTS 离线中文合成与扬声器播报。
- [`t_multinet/`](t_multinet/README.md)：N16R8 中文 MultiNet7 麦克风命令与意图识别。
- [`t_display/`](t_display/README.md)：ST7789T3 240×320 SPI LCD 背光与色条。
- [`t_touch/`](t_touch/README.md)：FT6336 检测和双点原始坐标。
- [`t_microphone/`](t_microphone/README.md)：数字/模拟麦克风左右声道电平对照、自动录音回放。
- [`t_audio/`](t_audio/README.md)：ES8311 + NS4150B 测试音、麦克风录音、电平与录音回放。
- [`t_mn316/`](t_mn316/README.md)：直接调用板级定义与 MN316 驱动的独立 AT 探测。
- `t_mn316_tcp/`、`t_mn316_stm32f103c8/`：历史 Zephyr 诊断，不参与主工程构建，
  根 build.ps1 不再为这些工程提供 Zephyr 环境。需要追溯时参考 archive/zephyr/。

根脚本可构建 ESP-IDF 子工程：`./build.ps1 -Application debug/t_mn316`。
子工程默认输出到各自目录下的 `build/`，主工程输出到根目录 `build/`。

音频子工程构建：`./build.ps1 -Application debug/t_audio`。

中文 TTS 子工程构建：`./build.ps1 -Application debug/t_tts`。
