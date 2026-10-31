# 架构与移植计划

沿用 EPOS 的应用、管理器、事件、驱动、共享模块分层，应用目录为根目录 `apps/`，系统框架位于 `system/`。
主控现改为 ESP32-S3，参考 DeepStoa 的 ESP-IDF 组件结构建立主工程；
显示屏、触摸和存储外设仍需按 Bepio 实板独立配置，不直接复制 EPOS 外设接线。

依赖方向：apps / system/launcher / system/services → system/appmgr、system/controller → drivers → ESP-IDF 驱动 API。
事件定义放入 system/events；需要多业务订阅时再引入事件分发机制。
UI 组件放入 system/uilv；明确显示控制器、内存预算后再接入 LVGL。
应用新增时采用 app、model、view、controller 分离，避免业务直接访问 UART。

## 当前实现

app_main 显式初始化服务、注册应用并打开 Settings，
然后执行 bepio_system_init → bepio_mn316_manager_init → mn316_probe → ESP-IDF UART1（GPIO17/18，外接 MN316）。
`drivers/mn316` 只配置 UART 并执行启动探测；`system/controller/bepio_mn316_manager`
提供启动入口和按需 API；`system/services/mn316_service` 串行化 UART 会话，
提供有界 AT、入网检查、OneNET 模组 MQTT 和 ESP32 TLS/MQTT 测试会话。
调用方提供认证信息；启动阶段只探测，不自动连接云端。

`system/appmgr/bepio_app_manager` 移植 EPOS 应用描述符、单应用生命周期、
返回拦截和可见状态回调，注册容量由 CONFIG_BEPIO_MAX_APPS 控制（默认 8）。
使用 ESP-IDF pthread 递归互斥锁序列化状态，生命周期回调中的变更请求返回 -EBUSY；
关闭通知在释放锁后执行，可以打开下一应用。UI 句柄暂为不透明指针，
调用方式为同步执行；接入 LVGL 时由 UI 线程调度并持有相应 GUI 锁。
与 EPOS 的 LVGL 定时器方式存在这些接口差异：

- API 与类型统一改为 bepio_ 前缀；注册与关闭操作返回错误码。
- start_func 返回 int，失败后调用 stop_func 清理部分初始化资源。
- 同一应用重复打开不重启；存在另一活动应用时返回 -EBUSY，需先关闭。
- delete 强制停止且不通知，exit_app 先交给 back_func，再决定停止并通知。
- 新增 set_ui_available，触发原有 UI 可用/不可用回调。

`apps/settings/` 分为静态应用描述符、RAM 偏好模型、控制器及日志 view。
ESP-IDF console REPL 是当前交互适配层，默认 UART0，可选原生 USB Serial/JTAG；已接入 LVGL launcher 与应用摘要视图；持久化及设置所对应的业务服务仍待接入。
其余功能目录仍为后续实现入口。上游来源与许可见 third-party.md。

第二个应用 `apps/contacts/` 同样分为 app/model/view/controller，提供姓名、号码与固定 ID
组成的联系人表，默认容量 32，采用静态内存和控制器互斥锁。
支持列表、详情、新增、修改、删除及详情返回列表；删除选中项自动返回列表。
Contacts 的增删改、收藏状态和稳定 ID 提交到 NVS，重启恢复；首次无快照时才注入可选示例数据。
底层 NVS blob 读写由 `system/platform/bepio_store.c` 适配，写入失败回滚内存修改。
两个应用通过 app_manager 互斥运行，
shell 不会强制关闭另一个应用；必须先从其主页/列表返回退出。

第三个应用 `apps/messages/` 实现 home/box/detail/compose/confirm 导航状态，
业务存储与收发状态由 `system/services/messaging/` 持有。应用 model 仅保存编辑器与确认动作，
关闭 UI 不删除消息。发送、删除通过明确确认执行；未保存返回先提供 save/discard/cancel。
contacts 新增只读 lookup 接口，可在 UI 未打开时读取，不改变其导航或状态。
联系人姓名、号码快照写入草稿与消息，历史记录不跟随联系人变化。

发送状态为 draft → queued → sending → sent/failed，failed 可回到 queued。
重试保留内容和消息 ID，每次发送增加 attempt，回执必须匹配这两个标识。
离线不领取队列，真实成功只能由服务端确认；显式模拟结果保存独立标记。
接收检查规范 UTF-8、容量和去重后入箱，详情查看才标记已读。
锁顺序为 app_manager → 应用 controller → messaging；服务不回调 UI。
网络线程、线协议、持久化、跨重启幂等标识及认证尚未实现。

## 后续顺序

1. 核对主控、晶振、模组供电与 UART，完成上板 AT 验证。
2. 为 MN316 增加独占接收线程、AT 命令队列、URC 分发及超时重试。
3. 实现注册入网与一种消息协议，补充解析、断网与重连测试。
4. 验证已接入的 ST7789T3 launcher 与 FT6336 触摸，继续完善应用编辑页面、消息列表与通知。
5. 实现已声明的 ES8311 音频方案并确认定位方案；STT/TTS、AI 过滤需单独设计端云协议，不能视为当前 MCU 已具备的能力。
