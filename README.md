# Bepio

基于 **ESP-IDF v5.5.3** 的 ESP32-S3 BB 机固件框架，外接 MN316 NB-IoT 模组。
目录与组件组织参考本机 `D:/Codes/DeepStoa`，业务应用位于根目录 `apps/`，系统能力位于 `system/`。
产品设想见 [PROJECT.md](PROJECT.md)。

## 目录

- `CMakeLists.txt`、`sdkconfig.defaults`、`partitions.csv`：ESP-IDF 构建与 16 MB Flash 配置。
- `main/Bepio.c`：`app_main()` 入口，显式初始化系统服务、注册应用并启动控制台。
- `boards/esp32s3/`：主板和 ES8311 + NS4150B 音频子板接线定义。
- `apps/`：Settings、Contacts、Messages，保留 app/model/view/controller 分层。
- `system/appmgr/`：应用生命周期；`system/controller/`：硬件与系统启动。
- `system/services/`：消息服务，以及音频、电源、定位服务预留目录。
- `system/platform/`：ESP-IDF 日志、递归互斥锁与可选 UART/USB 命令行适配。
- `system/uilv/`、`system/launcher/`：LVGL 显示触摸适配与上电 launcher。
- `system/events/`、`system/launcher/`、`system/uilv/`：事件、桌面与 UI 预留目录。
- `drivers/`：硬件驱动；`modules/`：后续共享库；`assets/`：资源。
- `debug/`：独立硬件调试工程；`tests/`：业务主机回归与后续整机测试。
- `refer/`、`external/`、`LICENSES/`：原有参考资料及许可，保持原样。
- `archive/zephyr/`：迁移前的源码、板定义与文档备份，不参与当前构建。

## 构建与烧录

本机 ESP-IDF 默认路径为 `C:/Espressif/frameworks/esp-idf-v5.5.3`。

```powershell
./build.ps1
./build.ps1 -Pristine
./build.ps1 -IdfPath C:/Espressif/frameworks/esp-idf-v5.5.3
# 独立演示构建，启用明确标注 SIMULATED 的消息命令
./build.ps1 -MessagesDemo
# 已有 ESP-IDF MN316 独立诊断工程
./build.ps1 -Application debug/t_mn316
# ESP-TTS 离线中文播报验证
./build.ps1 -Application debug/t_tts
```

也可激活 ESP-IDF 后使用标准命令：

```powershell
$env:IDF_TOOLS_PATH = 'C:/Espressif'
. C:/Espressif/frameworks/esp-idf-v5.5.3/export.ps1
idf.py build
idf.py menuconfig
idf.py -p COM5 flash monitor
```

默认构建产物为 `build/Bepio.bin`，演示构建在 `build-messages-demo/`。
演示固件烧录使用 `idf.py -B build-messages-demo -p COM5 flash monitor`。
COM5 替换为实际端口（当前调试使用 COM8）。默认日志与交互使用 UART0 桥，
TX=GPIO43、RX=GPIO44、115200。可在 menuconfig 切换至原生 USB Serial/JTAG（GPIO19/20）；
控制台适配层随所选接口创建 REPL。若 ROM 日志后没有输出，先确认监视端口与该配置一致。
`build.ps1` 只构建，不自动烧录。ESP-IDF 镜像有独立 bootloader、分区表和 app，
请使用 `idf.py flash`，不要沿用旧 Zephyr 单镜像烧录命令。

原 Zephyr `build/` 已保留为 `build-zephyr/`。其他保留的 Zephyr 构建目录为历史产物，
不作为 ESP-IDF 输入。当前源码无需 west、Zephyr SDK 或设备树。

## 接线与业务

MN316 保持 UART1 TX=GPIO17、RX=GPIO18、9600 8N1。
音频保持 SDA=41、SCL=42、MCLK=7、BCLK=4、DOUT=6、LRCK=15、DIN=16。
数据方向以音频子板为视角，详见 [硬件说明](docs/hardware.md)。
16 MB Flash 沿用实板检测结果；用户确认模组为 N16R8（8 MB Octal PSRAM）。
主工程仍未启用 PSRAM，独立 [t_multinet](debug/t_multinet/README.md) 为中文意图识别启用 PSRAM。

启动显式初始化消息服务、注册应用、打开 Settings，随后有界探测 MN316；
主工程上电自动点亮 ST7789T3 并显示 launcher，不需要输入命令。首页先于 MN316 探测显示，
未接模组或 AT 超时不阻止屏幕运行。默认关闭交互式 REPL，UART0 保留日志输出。

```text
settings show
settings set hour_24 0
settings about
settings back
settings back
contacts add "妈妈" 8613800000000
contacts list
contacts back
messages new 8613800000000 "妈妈"
messages text "我到家了。"
messages send
messages confirm
messages outbox
```

每个应用只允许单独打开，先从当前应用主页返回退出再切换。
`settings help`、`contacts help`、`messages help` 可列出子命令。
联系人、偏好和消息均暂存 RAM，重启清空。普通消息保留队列，
真实网络传输适配器尚未接入。独立 [t_tts](debug/t_tts/README.md) 已提供 ESP-TTS 中文播报验证，主工程尚未自动播报收到的消息。主工程音频服务尚未初始化 codec 或启动 I2S；独立 [t_audio](debug/t_audio/README.md) 可测试播放、录音和回放。

## 验证

```powershell
python tests/unittest/app_manager/run.py
python tests/unittest/contacts/run.py
python tests/unittest/messages/run.py
python tests/unittest/console/run.py
```

验证记录与迁移边界见 [ESP-IDF 迁移说明](docs/espidf-migration.md)。
主机测试验证业务状态，不替代真实 FreeRTOS 调度、USB 控制台及模组上板测试。

## 屏幕启动

主工程默认进入240×320竖屏 launcher，使用独立 t_display 已验证的RGB色序和背光高电平。
FT6336触摸可选，初始化失败保留首页。Contacts 支持带 mock 数据的触摸增删改查；其余应用可看基础图形摘要并返回首页。
串口REPL默认关闭；图形模式下不启用串口业务操作，避免跨线程调用LVGL。
LVGL由组件管理器获取并锁定9.5.0，主固件不依赖任何 debug 工程或 NomadCast 绝对路径。
[主机渲染预览](docs/assets/launcher.png) 不替代实板显示和触摸验证。
