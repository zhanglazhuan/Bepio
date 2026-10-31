# t_tts：ESP-TTS 离线中文播报

独立 ESP-IDF v5.5.3 工程。使用官方 ESP-SR 2.4.7 附带的 ESP-TTS 与“小新”精简音色，
复用 `t_audio` 的 ES8311 + NS4150B 驱动，接线统一引用 `boards/esp32s3/bepio_audio.h`。
当前 MCLK=GPIO7、SDA=41、SCL=42、BCLK=4、LRCK=15、codec DOUT=6、codec DIN=16。
不启动 MN316、主工程应用、STT、唤醒词或网络。

## 使用

从仓库根目录构建：

```powershell
./build.ps1 -Application debug/t_tts
```

在已经激活 ESP-IDF 的终端中烧录、监视：

```powershell
idf.py -C debug/t_tts -B debug/t_tts/build -p COM5 flash monitor
```

COM5 替换成实际原生 USB 端口。使用开发板的 USB 接口（GPIO19/20），
日志和命令行不走 UART0 COM 桥。连接模块 5V、GND 与扬声器，IO 电平为 3.3V。
本工程烧录会替换当前应用和分区表；恢复主工程时重新烧录主工程。

**首次必须执行 `flash`，不要只执行 `app-flash`。** CMake 已将语音数据加入烧录列表：

| 内容 | 地址 | 空间 |
|---|---|---|
| bootloader | 0x0 | 标准 ESP-IDF 布局 |
| 分区表 | 0x8000 | 标准 ESP-IDF 布局 |
| t_tts 应用 | 0x10000 | 2 MiB |
| voice_data 原始语音数据 | 0x210000 | 3 MiB |

语音文件由 Component Manager 下载的 `esp-sr` 提供，无需手动拷贝或烧录模型。
启动检查 SHA-256；若报告不匹配，重新执行完整 `flash`。

启动自动初始化，但不自动发声。等待 `Done`，在 `tts>` 控制台运行：

```text
tts status
tts demo
tts say "妈妈说，今天下午五点回家。"
tts say "你有一条新消息，请注意查看。"
```

每次等待 `Done` 后再输入下一条播放命令。其他控制：

```text
tts speed 3
tts volume 60
tts stop
tts init
```

- `speed`：0 最慢，5 最快，默认 3。
- `volume`：0–100，默认 60。
- `stop`：请求在当前合成/输出块边界停止，随后恢复静音。
- `init`：初始化失败时，检查供电和接线后重试。
- `status`：查看语音数据和音频设备状态、内部空闲堆、最大连续空闲块和上次播放样本数。

输入为 UTF-8 中文，最多 256 字节，文本包含空格时应使用双引号。
只保证当前引擎支持的中文；字母、表情及特殊符号的读法需单独验证。
目前是拼接式音色，不是大模型自然语音或克隆音色。

## 工作方式

`UTF-8 文本 → esp_tts_parse_chinese → esp_tts_stream_play → mono PCM → L/R slot → ES8311 → NS4150B`。
合成输出与 codec 均为 16 kHz / 16-bit，不需要重采样。
后台单任务串行合成和播放，避免阻塞控制台；只保留少量 PCM 块，不缓存整段音频。
初始化、输入校验、模型校验与流状态封装在根目录
[`system/services/tts`](../../system/services/tts/README.md)，以后可供消息播报调用。

PSRAM 当前仍未启用；实际运行时内存是否足够以 `tts status` 和实板结果为准。
主工程尚未接入自动播报；此子工程先验证“输入中文 → 扬声器发声”的链路。

## 验证

```powershell
python debug/t_tts/tests/run.py
```

2026-10-30：ESP-IDF v5.5.3 完整构建通过，应用镜像 406,688 字节，
语音数据 2,913,777 字节；已检查 `debug/t_tts/build/flash_args` 包含语音数据。
主机测试覆盖 UTF-8/长度、模型校验、流输出、取消、输出错误、状态 reset 和调用互斥。
尚未烧录或进行实板发音、音量、内存峰值验证。

官方参考：[ESP-TTS](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/speech_synthesis/readme.html)、
[ESP-SKAINET 中文示例](https://github.com/espressif/esp-skainet/tree/master/examples/chinese_tts)。
