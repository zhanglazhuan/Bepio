# messaging

消息收发箱、草稿、发送队列与接收去重服务。应用关闭后继续保留数据；主固件将文本快照持久化到 Storage SPIFFS。
所有读写由 ESP-IDF pthread 递归互斥锁保护，不向调用方暴露内部可变指针。

## 传输适配边界

`messaging.c` 负责存储；`system/mn316_ctrl/bepio_mn316_manager.c` 提供后台 MN316 EMQX 适配器，
由 `bepio_mn316_manager_emqx_start()` 启动。UART/TCP/TLS 在 `drivers/mn316`，MQTT 与主题规则在 `system/mn316_ctrl`。
适配器遵循以下状态循环（旧五字段协议尚不含独立的服务端幂等键）：

1. 连接状态变化时调用 `bepio_messaging_set_online()`；AT 回应不等于网络可用。
2. 在线后调用 `bepio_messaging_begin(false, &message)`，领取最早的待发送记录。
3. 将设备/会话唯一标识与本地消息 ID 组成服务端幂等键；重试复用同一键及内容。
4. 收到与本次尝试匹配的服务端 ACK 后调用 `complete(id, attempt, false, true)`。
5. 明确错误或确认超时调用 `complete(..., false)`，进入失败/未确认；不自动重复发送。
6. 接收数据通过 `receive(remote_id, payload, epoch_ms, false, &id)` 入箱。
   成功或已去重才可确认收到；满箱返回 `-ENOSPC`，不能确认后丢弃。

同一时刻只允许一条 sending；离线领取返回 `-ENETDOWN`，排队内容不丢失。
旧 ID/attempt/模拟标记不匹配的回执返回 `-ESTALE`，不能覆盖新的尝试状态。
queued 可取消删除，sending 不可删除；排队后不可编辑，失败只能按原内容重试。
已发送表示服务器确认，不表示对方已收到或已读。

## 容量、时间和去重

收发箱各默认 20 条，由相应 Kconfig 控制。所有已保留收件始终去重，
另保留最近 inbox-capacity 个接收标识以抑制删除后的重投；更久的删除记录不保证去重。
去重键为发送人号码、remote_id、模拟标记，真实与模拟接收互不污染。
时间由适配器提供，0 表示未同步；服务不会把启动后毫秒数冒充真实日期。

主固件的消息 ID、草稿及去重窗口随文本快照跨重启恢复。目前 EMQX 使用 TLS MQTT 认证，后台处理 PUBACK、30 秒发送确认超时、心跳和断线重连。
旧五字段协议用规范 JSON 的 SHA-256 前 20 字节生成本地收件去重键；同一秒同内容消息可能合并。
仍需服务端幂等键和设备身份/ACL 绑定才能作为正式可靠消息协议。
PUBACK 仅证明 broker 接收；超时置失败，手动重试可能导致对端重复收到。

## 测试入口

`CONFIG_BEPIO_MESSAGES_DEMO` 在普通构建中默认开启，可关闭；演示数据允许模拟入箱和模拟队列结果；
每条模拟记录始终显示 `SIMULATED-*`，不会显示为真实服务端 ACK。
