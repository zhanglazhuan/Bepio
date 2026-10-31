# unittest

`app_manager/` 包含应用管理器和 Settings 的主机测试：

```powershell
python tests/unittest/app_manager/run.py
```

使用实际业务源码和业务平台主机适配头，验证单线程生命周期与偏好行为。
FreeRTOS 调度、真实互斥与板级行为仍需目标系统测试。

`contacts/` 验证联系人增删改查、输入与容量边界、固定 ID、应用生命周期和 shell 命令处理：

```powershell
python tests/unittest/contacts/run.py
```

`messages/` 验证消息服务、确认流程、草稿、收件去重、队列、回执重试、
联系人快照及 shell 处理函数：

```powershell
python tests/unittest/messages/run.py
```

`console/` 编译真实 ESP-IDF 控制台适配源码，仅替换 SDK 入口，验证命令注册、参数转发、必选/可选数量、帮助与错误码：

```powershell
python tests/unittest/console/run.py
```


`display_async/` 编译真实 ST7789 驱动，仅替换 SDK：检查借用缓冲、忙状态、非阻塞轮询、
完成信号回收、同步兼容入口、提交失败、超时与晚到ISR后禁止重用。

```powershell
python tests/unittest/display_async/run.py
python tests/unittest/orientation/run.py
```

orientation 回归覆盖全部触摸坐标、旋转互逆，以及不同长宽（含奇数宽度）的融合字节序转换和源缓冲不变。
