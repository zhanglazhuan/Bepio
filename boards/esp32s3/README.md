# Bepio ESP32-S3 DevKitC

`bepio_devkit.h` 定义主板，`bepio_audio.h` 定义 ES8311 + NS4150B 音频子板。
`bepio_display.h` 定义 2.8 寸 SPI LCD + I2C 触摸子板，接线见
[显示子板说明](../../docs/display-board.md)。
作为 ESP-IDF 头文件组件注册，驱动通过组件依赖包含头文件；不再使用 DTS/shield。

接线、电源、GPIO48 RGB LED 占用和待确认项见 [硬件说明](../../docs/hardware.md)。
只迁移 Bepio 接线，不继承 DeepStoa 的屏幕、触摸、电源等不同硬件配置。
