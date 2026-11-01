# ESP-TTS 中文合成服务

封装官方 ESP-SR 2.4.7 附带的 ESP-TTS，通过组件依赖引用，不复制或修改乐鑫引擎。
当前使用 `esp_tts_voice_data_xiaoxin_small.dat`，中文 UTF-8 输入、16 kHz / 16-bit / 单声道 PCM 流输出。
采用拼接式语音合成，语速 0–5（慢到快），默认测试工程使用 3；不承诺英语或任意外语朗读。

## 接入

独立验证见 [`debug/t_tts`](../../../debug/t_tts/README.md)。
主工程 Messages 点击中文语音条时由后台音频线程实时合成并通过 ES8311 播放，不保存 PCM。音色使用独立 3 MiB 分区，默认 `flash` 暂不写入模型，保留 Flash 中已有音色。实际发声、内存余量与连续播放仍待实板验证。

```c
ESP_ERROR_CHECK(bepio_tts_init());
const bepio_tts_output_t output = {
    .write = audio_pcm_write, /* 用户实现：将 mono PCM 流交给音频设备 */
    .cancelled = audio_cancelled, /* 可选 */
    .context = audio_context,
};
esp_err_t result = bepio_tts_speak(message_text, 3, &output, NULL);
```

`write` 回调必须同步消费当前 PCM，不能保存指针或释放引擎缓冲。
服务不拥有 I2S/codec，不改变硬件音量；测试工程将 mono 样本复制到 L/R slot。
`speak` 是阻塞调用，不能从 MN316 UART 接收任务直接执行。
同一时刻只能有一个 init/speak/deinit；忙时返回 `ESP_ERR_INVALID_STATE`。

## 资源与错误

语音数据映射 Flash，不先复制整个音色到 RAM。启动校验模型 SHA-256，
拒绝缺失、未烧录或损坏的数据。需要烧录模型时，先执行 `idf.py -DBEPIO_FLASH_TTS_MODEL=ON reconfigure`，再执行 `idf.py flash`；完成后用 `idf.py -DBEPIO_FLASH_TTS_MODEL=OFF reconfigure` 恢复跳过。
`voice_data` 虽使用 FAT subtype，但存放原始 `.dat`，不能作为 FATFS 挂载或格式化。

输入上限 256 字节，检查完整 UTF-8、空白输入和控制字符；这与当前消息正文上限相同。
上层若需要更长正文，应按中文标点分段排队，不能直接截断 UTF-8 字节。
解析无结果返回 `ESP_ERR_NOT_SUPPORTED`，输出失败返回回调错误，取消返回
`ESP_ERR_NOT_FINISHED`；成功、失败和取消后都会 reset 流状态，以便下一条消息合成。
循环总时限 60 秒，在引擎块边界检查；取消同样在块边界和输出回调中检查，
不会强行打断正在执行的闭源引擎函数。

测试工程未启用 PSRAM；通过 `tts status` 检查实际内部堆和最大连续空闲块。
主机测试替换引擎与 Flash 入口，只验证封装行为，不验证真实发音或设备内存峰值。

官方资料：[ESP-TTS 中文说明](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/speech_synthesis/readme.html)、
[中文示例](https://github.com/espressif/esp-skainet/tree/master/examples/chinese_tts)。
