# 第三方来源

`system/appmgr/bepio_app_manager.c` 与 `.h` 改编自
`D:/Codes/EPOS/epos/managers/epos_app_manager.c`、`.h`。
上游源文件注明 EPOS / ZSWatch 来源并采用 GPL version 3；移植保留原始版权和许可声明。
完整许可证见 `LICENSES/GPL-3.0-only.txt`。该说明不改变 `refer/` 中厂商资料的原始许可。

本次保留注册表、单应用生命周期、返回拦截、关闭通知和可见状态回调；
移除 LVGL 定时器、未使用的 ZBUS 引用及固定 25 应用限制，替换为同步调用和 Kconfig 容量。
新增注册校验、并发互斥、回调重入保护及启动失败清理。

ESP-TTS 由 `system/services/tts/idf_component.yml` 固定依赖官方 `espressif/esp-sr 2.4.7`，
音频设备使用官方 `espressif/esp_codec_dev 1.6.2`。组件由 ESP-IDF Component Manager 获取，
保留其软件包中的许可证与版权声明；没有复制 ESP-SKAINET 的板级驱动。
ESP-TTS 接口与小新语音数据的使用方式参考官方 chinese_tts 示例。
