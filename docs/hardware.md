# 硬件定义

主控为 ESP32-S3 DevKitC，MN316 为 UART 外接 NB-IoT 模组。
ESP-IDF 板级头文件为 [`bepio_devkit.h`](../boards/esp32s3/bepio_devkit.h)，
音频接线为 [`bepio_audio.h`](../boards/esp32s3/bepio_audio.h)。

| 用途 | 控制器 / 接线 |
|---|---|
| MN316 | UART1 TX GPIO17 → RXD；RX GPIO18 ← TXD；9600 8N1 |
| 控制台 | 默认 UART0，TX43/RX44，115200；可选原生 USB Serial/JTAG GPIO19/20 |
| UART0 桥 | 开发板 COM 接口用于烧录及默认应用日志/交互 |
| Flash | 实板检测为 16 MB，DIO；分区见根目录 partitions.csv |
| CPU | 240 MHz；PSRAM 规格未确认，未启用 |
| 音频控制 | I2C0 SDA GPIO41 / SCL GPIO42，100 kHz |
| 音频主时钟 | I2S0 MCLK GPIO7 |
| 音频位时钟 | I2S0 BCLK GPIO4 → 子板 SCLK/BCK |
| 音频帧时钟 | I2S0 WS GPIO15 → 子板 LRCK |
| 录音 | 子板 DOUT/DO → GPIO6，主控 RX |
| 播放 | GPIO16 → 子板 DIN/DI，主控 TX |

MN316 与主控需共地，并按手册提供独立、足够的电源。尚未定义 PWRKEY/RESET，
需预先供电开机。MN316 驱动只执行有界 AT 探测，不等于完成入网或消息收发。

## ES8311 + NS4150B 子板

当前板级头文件已将 MCLK 定义为 **GPIO7**，其余音频接线继承原定义。
部分 DevKitC 的 GPIO48 连接板载 RGB LED，因此本测试按当前头文件使用 GPIO7。GPIO41/42 占用外部 JTAG 的 MTDI/MTMS，
当前 USB Serial/JTAG 不受影响；若需要外部 JTAG，可统一规划 SDA=8、SCL=9。
显示屏、触摸和定位模块的接线仍未确定。

ES8311 的 I2C 地址为 0x18（CE=0）或 0x19（CE=1），当前声明暂按 0x18，
仍需扫描确认。子板 5V 端按手册建议接 5V；3.3V 供电时功放输出能力降低。
IO 与 I2C 上拉电平应为 3.3V，不能将 ESP32-S3 GPIO 拉到 5V。
子板与主板共地，I2C 线按厂商建议不超过 5 cm，音频时钟线尽量短。
NS4150B 是模拟功放，接口未引出独立 PA enable，因此没有定义功放控制 GPIO。

主工程音频服务当前仍只有接线常量；独立 [t_audio](../debug/t_audio/README.md)
使用 ESP-IDF I2C/I2S 和官方 ES8311 驱动测试播放、录音及回放。
主控提供 MCLK/BCLK/LRCK，录放音使用一致的采样率和 slot 格式。
上板需验证电源、I2C ACK、时钟波形、播放、录音及同时录放音。

依据：本机 `D:/Dev/【鹿小班】ES8311+NS4150B Codec音频模块客户资料` 中的模块说明书与 ES8311 数据手册。
官方参考：[I2S](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/peripherals/i2s.html)、
[DevKitC 修订说明](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)。

`refer/YNH-MN316/` 厂商 STM32 资料原样保留，不能直接使用其 MCU 引脚或 HAL 配置。
