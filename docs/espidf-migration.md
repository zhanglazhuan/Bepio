# ESP-IDF 迁移记录

2026-10-30：主工程由 Zephyr 4.3.0 迁移到 ESP-IDF v5.5.3。
结构参考 `D:/Codes/DeepStoa` 的根工程、main 入口、boards 头文件、drivers 与独立组件。
没有复制 DeepStoa 的业务、外设引脚、PSRAM 配置或第三方驱动。

| 原实现 | 当前实现 |
|---|---|
| bepio/CMakeLists.txt 是应用构建入口 | 根 CMakeLists.txt 包含 ESP-IDF project.cmake |
| bepio/main.c 的 main | main/Bepio.c 的 app_main |
| west / prj.conf | idf.py / sdkconfig.defaults / Kconfig.projbuild |
| boards DTS、pinctrl、audio shield | boards/esp32s3/bepio_devkit.h 与 bepio_audio.h |
| SYS_INIT | app_main 显式初始化服务、注册应用 |
| Zephyr 递归互斥锁 | ESP-IDF pthread 递归互斥锁，底层使用 FreeRTOS |
| Zephyr LOG | ESP-IDF esp_log，经 bepio_log.h 调用 |
| Zephyr shell | ESP-IDF esp_console USB Serial/JTAG REPL，保留业务子命令 |
| Zephyr UART 轮询 | ESP-IDF UART 驱动，固定缓冲及有界 AT 等待 |

业务目录与 app/model/view/controller 分层、联系人 CRUD、偏好、消息草稿、
确认发送/删除、队列、ACK/attempt 校验、模拟标记等规则保留。
应用描述符上游 EPOS/ZSWatch 的 GPL 声明和 LICENSES 保留。

原主工程源码和构建定义备份到 `archive/zephyr/`，原 build 改名 `build-zephyr/`。
历史 debug/t_mn316_tcp、debug/t_mn316_stm32f103c8 是 Zephyr 独立诊断资料，
不参与当前主工程；debug/t_mn316 保持原有独立 ESP-IDF 工程。
根目录 AGENTS.md 已同步为 ESP-IDF 指南，旧版保存在 archive/zephyr/AGENTS.md。

根目录分区为 16 MB：NVS、PHY、2 MB factory app、剩余 storage 预留分区。
当前未挂载 storage，没有自动持久化或 OTA。新固件使用 ESP-IDF 标准多镜像烧录，
不能沿用 Zephyr 单镜像地址命令。

## 根目录拆分

后续按 DeepStoa 目录布局移除主工程的 `bepio/` 包装层：

| 原 ESP-IDF 路径 | 当前路径 |
|---|---|
| bepio/apps | apps |
| bepio/managers/bepio_app_manager.* | system/appmgr/bepio_app_manager.* |
| bepio/managers/bepio_system_manager.* | system/controller/bepio_system_manager.* |
| bepio/services | system/services |
| bepio/platform | system/platform |
| bepio/events、launcher、bepio_lv | system/events、launcher、uilv |
| bepio/main.c、bepio.h | 启动逻辑合并至 main/Bepio.c 的 app_main |
| bepio/Kconfig.projbuild | main/Kconfig.projbuild |

根 CMake 显式注册各应用和系统组件，原 bepio、managers 组件由
main、appmgr、controller 替代。业务 API、接线和运行流程保留。
历史 Zephyr 备份目录中的 bepio 路径保持原样。

## 验证与边界

- ESP-IDF v5.5.3 主工程完整构建通过，生成 build/Bepio.elf、Bepio.bin、bootloader 与分区表。
- 独立 MessagesDemo 配置完整构建通过；默认配置未启用模拟命令。
- 关闭三个应用、MN316 探测及控制台的最小配置构建通过（build-minimal/）。
- 应用管理器/Settings、Contacts、Messages 及新控制台适配层四组主机回归通过。
- 主机锁适配是单线程测试，不证明 ESP-IDF 的多任务时序正确。
- 尚未烧录；USB 输入、UART 超时及真实响应、蜂窝入网需实板验证。
- 音频只迁移接线定义，尚无 codec 初始化与录放音实现。
- 消息网络工作线程尚未接入，正常提交只入队，不冒充真实发送成功。
