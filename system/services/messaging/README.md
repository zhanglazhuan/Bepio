# messaging

消息收发箱、草稿、发送队列与接收去重服务。应用关闭后继续保留数据；主固件将文本快照持久化到 Storage SPIFFS。
所有读写由 ESP-IDF pthread 递归互斥锁保护，不向调用方暴露内部可变指针。

## 传输适配边界

`messaging.c` 负责存储；`system/mn316_ctrl/bepio_mn316_manager.c` 提供后台 MN316 EMQX 适配器，
由 `bepio_mn316_manager_emqx_start()` 启动。UART/TCP/TLS 在 `drivers/mn316`，MQTT 与主题规则在 `system/mn316_ctrl`。
适配器遵循以下状态循环：

1. 连接状态变化时调用 `bepio_messaging_set_online()`；AT 回应不等于网络可用。
2. 在线后调用 `bepio_messaging_begin(false, &message)`，领取最早的待发送记录。
3. 新消息创建时生成 UUID v4，存入 remote_id 并作为线上 Id；重试复用同一 Id、创建时间及内容。
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
Version 1 八字段协议直接使用线上 Id 作为收件 remote_id，按 From + Id 去重；同一秒发送相同内容的两条新消息具有不同 UUID。
本地数值 id 用于界面索引，与线上 UUID 分开；仍需设备身份/ACL 绑定和应用层回执完善端到端可靠性。
PUBACK 仅证明 broker 接收；超时置失败，手动重试可能导致对端重复收到。

## 测试入口

`CONFIG_BEPIO_MESSAGES_DEMO` 在普通构建中默认开启，可关闭；演示数据允许模拟入箱和模拟队列结果；
每条模拟记录始终显示 `SIMULATED-*`，不会显示为真实服务端 ACK。

UUID、重试保持与旧存储迁移回归：`python tests/unittest/messages/run.py`、`python tests/unittest/messages_storage/run.py`。
协议与 MQTT 回归：`python tests/unittest/mn316_mqtt/run.py`。

MN316 接收缺少 Id 的消息时，适配器先按 [From, To, Text, Datetime] 的紧凑 JSON 数组计算 SHA-256，生成 legacy: 加前 16 字节十六进制的稳定标识；再交给 receive 按发送方与该标识去重并持久化。不会使用接收时间重新生成随机 ID。
