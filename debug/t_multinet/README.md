# MultiNet 中文意图测试

独立 ESP-IDF v5.5.3 工程，使用 ESP-SR 2.4.7 的中文 MultiNet7。
目标模组为 ESP32-S3 N16R8：16 MB Flash、8 MB Octal PSRAM；本工程启用
40 MHz Octal PSRAM，主工程配置不变。识别预定义命令，输出意图 ID；
不提供任意内容听写，不调用 MN316，也不触发 TTS 或业务操作。

## 构建与烧录

在仓库根目录执行：

```powershell
./build.ps1 -Application debug/t_multinet
```

在已经激活 ESP-IDF 的终端中，从仓库根目录烧录：

```powershell
idf.py -C debug/t_multinet -B debug/t_multinet/build -p COM7 flash monitor
```

将 COM7 改为实际 USB Serial/JTAG 端口。标准 `flash` 同时烧录 bootloader、
分区表、应用和 `debug/t_multinet/build/srmodels/srmodels.bin`；首次使用不能只
`app-flash`。此工程的分区表与 t_tts、主工程不同，切换工程需要完整烧录。
模型由 ESP-SR 组件自动打包到 `model` 分区，不依赖 DeepStoa 或外部路径。

## 使用

USB 控制台出现 `mn>` 后执行：

```text
mn status
mn commands
mn init
mn listen
mn stop
```

`mn init` 只加载模型；`mn listen` 也会按需加载。看到 `Listening ... speak now`
后再说话，默认监听 8 秒，命中第一条命令即结束。没有命中时打印 `no match`。
`mn stop` 请求停止，待当前 I2S 读取和推理完成后生效。模型加载期间不能中断。

| ID | 意图 | 可说的预设命令 |
|---|---|---|
| 1 | send_location | 发送位置 / 告诉妈妈我在哪里 |
| 2 | request_pickup | 请来接我 / 来接我回家 |
| 3 | report_safe | 我已经到家了 |
| 4 | play_messages | 播放消息 / 读一下消息 |
| 5 | cancel | 取消操作 |

示例输出格式（非实测结果）：

```text
intent=send_location command_id=1 confidence=0.900 canonical_text=发送位置
```

`canonical_text` 是意图的标准文本，不是逐字转写；同一意图的不同说法输出相同文本。
修改 `main/t_multinet.c` 的 `phrases` 表即可扩展命令，中文注册使用无声调拼音，
同义命令复用 ID。模型输出的 confidence 用于调试，不代表已验证的业务准确率。

## 麦克风与接线

复用 `boards/esp32s3/bepio_audio.h` 和 `debug/t_audio/drivers/audio_board`：
SDA=41、SCL=42、MCLK=7、BCLK=4、LRCK=15、ES8311 DOUT→GPIO6、DIN←GPIO16。
16 kHz、16 位、立体声 I2S，选取一个时隙转为单声道，严格按模型要求的样本数读取。
扬声器保持静音；每次结束释放音频设备，模型留在内存供下一次使用。

默认模拟麦克风、左时隙、24 dB 增益。可调整：

```text
mn listen 8 right analog 24
mn listen 8 left digital 24
mn listen 8 left analog 36
```

完整语法：`mn listen [1..8秒] [left|right] [analog|digital] [0..42增益，步长6]`。
模拟/数字选项必须对应实际麦克风硬件。结束时输出 peak，接近 0 应先用
`debug/t_audio` 确认接线、时隙和录音；持续接近 32768 应降低增益并排查削波。

当前为安静环境的直接 PCM → MultiNet 基础链路，未加入 AFE 降噪、AEC、VAD
或 WakeNet；需手动开始每轮测试。生产场景再加入音频前处理、唤醒机制和业务确认，
不能把任意未收录语句的模型匹配视为可靠意图理解。

官方资料：[命令词与运行时 API](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/speech_command_recognition/README.html)。

## 验证边界

构建验证不代表硬件识别效果。上板需检查 PSRAM 启动日志、模型加载、左右时隙、
各命令及同义说法、静音和无关语句、停止与重复监听。尚未执行硬件录音或识别测试。
