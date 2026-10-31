# t_microphone：独立麦克风诊断

ESP-IDF v5.5.3，ESP32-S3 N16R8，16 MB Flash，不启用 PSRAM。
复用 `../t_audio/drivers/audio_board`，以已成功播放的 t_audio 配置为基准：
原生 I2C0 100 kHz、16 kHz/16-bit stereo、MCLK384×=6.144 MHz、音量60、输入增益24dB。
软件 I2C 默认 OFF，32-bit 实验已撤回，避免在本轮混入未经验证的配置差异。

## 自动流程

1. DIGITAL 模式初始化，静音并丢弃1秒启动数据，再测1秒底噪。
2. 倒计时3、2、1，听到高音结束后对麦克风说话3秒。
3. 低音表示录音结束，停止说话；打印左右声道 RMS、dBFS、峰值、最小/最大值、非零与削波数。
4. 选择 AC RMS 较强声道回放；RMS<4 时跳过并打印寄存器。
5. 切换 ANALOG 模式重复上述流程；结束静音关闭，RESET 可重跑。

初始化或采集失败时打印 NO COMPLETE RECORDING；最终汇总完成PCM采集的模式数，
这不代表已录到有效语音。此前上板回放仅杂音，语音输入仍未验证成功。
回放去直流、按 RMS 计算监听增益，最多32倍，并限制峰值；不会改动原始录音统计。
如果讲话 RMS 未达到底噪两倍会提示；放大也可能只放大噪声。

三秒双声道录音占用192KB内部RAM，模式切换/结束清除，不写Flash、不上传。
无命令行、无实时麦克风外放。UART0/CH340 输出日志。

## 接线

保持 `boards/esp32s3/bepio_audio.h` 接线：SDA41、SCL42、MCLK7、BCLK4、LRCK15、
DOUT/RX6、DIN/TX16。模块供电/共地、扬声器保持原连接。无 PA 控制脚。

## 构建

```powershell
# 仓库根目录
./build.ps1 -Application debug/t_microphone
# 子工程目录，显式更新已有实验配置缓存
idf.py -D BEPIO_AUDIO_SOFT_I2C=OFF -D BEPIO_AUDIO_SAMPLE_BITS=16 build
idf.py -p <PORT> flash monitor
```

初次构建由 Component Manager 获取 esp_codec_dev == 1.6.2。
原生 I2C 的 t_audio 已有用户成功播放记录；不能因本工程一次失败就认定原生 I2C 不可用。
本轮以其已验证格式作对照，编译不能证明录音有效，需上板比较讲话响应和回放。
