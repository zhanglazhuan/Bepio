# System

参照 DeepStoa，将长期运行的系统能力与应用分开。

- `controller/`：系统硬件启动，调用 MN316 探测并提供按需会话入口。
- `appmgr/`：应用注册、生命周期与导航回调。
- `services/`：消息、ESP-TTS 合成服务，以及音频、定位、电源服务预留目录。
- `platform/`：ESP-IDF 日志、递归互斥锁、USB 控制台适配。
- `events/`：事件接口预留。
- `launcher/`：桌面与应用启动入口预留。
- `uilv/`：共享 UI 层预留。

`main/Bepio.c` 显式初始化服务、注册根目录 `apps/` 下的应用并启动控制台。
应用使用服务和管理器接口，不直接初始化 UART、I2C 或 codec。
硬件接线统一由根目录 `boards/` 定义，硬件驱动保留在 `drivers/`。
