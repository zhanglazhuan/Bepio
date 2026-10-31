# t_audio：ES8311 + NS4150B 自动音频测试

独立 ESP-IDF v5.5.3 工程，使用 `espressif/esp_codec_dev == 1.6.2`。
当前默认使用原生硬件 I2C0（100 kHz），保持原 GPIO 接线；软件 I2C 作为可选对照保留。
默认完整初始化，启动等待 1.5 秒后自动执行，无需输入命令：
初始化 codec → 1 kHz 测试音 2 秒 → 静音检测数字麦克风电平 3 秒。
失败打印错误；启动初始化失败时不执行音频测试。原有控制台可用于额外诊断。

## 接线

接线由 `boards/esp32s3/bepio_audio.h` 定义，不使用 PSRAM。

| 模块信号 | ESP32-S3 GPIO |
|---|---:|
| SDA | 41 |
| SCL（I2C） | 42 |
| MCLK | 7 |
| SCLK/BCLK | 4 |
| LRCK/WS | 15 |
| DOUT（ESP RX） | 6 |
| DIN（ESP TX） | 16 |
| 5V/GND | 模块供电、共地 |

I2C 上拉为 3.3 V。模块没有引出 PA enable。
UART0 默认 TX43/RX44、115200；也支持 menuconfig 的原生 USB Serial/JTAG 控制台。

## 构建

```powershell
# 仓库根目录
./build.ps1 -Application debug/t_audio
# 子工程目录，指定实际端口
idf.py -p <PORT> build flash monitor
```

默认入口 `main/t_audio.c`。软件/原生 I2C 对照开关：

```powershell
# 软件 I2C 对照
idf.py -D BEPIO_AUDIO_SOFT_I2C=ON build
# 当前默认：原生 I2C，烧录前确保最后构建的是此分支
idf.py -D BEPIO_AUDIO_SOFT_I2C=OFF build
```

软件版本日志含 `audio_soft_i2c: Software I2C`，不开启硬件 I2C 控制器。
SDA/SCL 为开漏输入输出、逻辑 1 释放线路；每半周期至少延迟 5 us，实际速率低于 100 kHz，
任务中断会拉长时钟周期，不保证精确频率。SCL 释放后最多等待 1 ms，未拉高则超时。
逐字节报告地址/寄存器/数据 NACK；发送 1 却采样为 0 时报告具体字节与位号并停止该事务。
寄存器读使用 repeated START；最后一个接收字节发送 NACK。无自动重试。
软件对照只能区分不同传输实现的表现，成功也不能独立证明原生驱动有缺陷。
只读 A/B/C 抓取程序 `main/t_audio_capture.c` 保留：

```powershell
idf.py -D BEPIO_AUDIO_CAPTURE_ONLY=ON build
# 恢复完整音频测试
idf.py -D BEPIO_AUDIO_CAPTURE_ONLY=OFF build
```

## 完整初始化波形采集

分析仪并联 SDA/SCL/GND，保留原接线。上次 CSV 实际通道为 SDA=Channel1、SCL=Channel2。
设置 I2C 7 位地址解码、10 MS/s、记录 10 秒；先开始采集再按 RESET。
日志标记 codec 寄存器初始化、格式设置和启用阶段，失败包含寄存器、值、原生错误码。
不要在采集时输入额外命令。新 CSV 和同次串口日志需配套分析。

只读模式 A/B/C 分别为时钟关闭、完整时钟开启、关闭并恢复 I2C。
每阶段三轮：探测 0x18/0x19，读取 FD/FE；预期 ID 为 83/11。
用户提供的上一轮 CSV 中三阶段均通过；这只验证地址与 ID 读取，未验证配置写入。

## 当前配置与参考

参考 WeActStudio 示例的顺序和时钟参数：先启用 I2S TX/RX，再初始化 I2C/codec；
原生 I2C100kHz（软件版本实际速率见上文），16kHz/16-bit stereo，MCLK384×=6.144MHz，外部 MCLK，codec 为 slave。
保留本板数字麦克风和引脚，不采用参考板的 PA 控制接线。

- https://github.com/WeActStudio/WeActStudio.ES8311MonoAudioCodecModule/tree/master/Examples/i2s_es8311
- 固定参考提交：7294d8730b64c724ab25bfc72218a7e7168ede86。
- 厂家资料：`D:/Dev/【鹿小班】ES8311+NS4150B Codec音频模块客户资料`，含文档和固件，无 C/C++ 源码。

本地 `audio_i2c_ctrl.c` 基于官方 1.6.2 adapter，保留 Apache-2.0 声明；
传输行为一致，仅补充错误日志，不修改 managed_components。
编译不能证明音频硬件正常；完整初始化、播放、录音及通信稳定性仍需上板验证。

换板后的软件 I2C 初始化与测试音播放已由用户确认通过；原生 I2C 本轮恢复验证，
启动日志应含 `Native hardware I2C0`。麦克风此前接近静音，仍未验证有效采音。
