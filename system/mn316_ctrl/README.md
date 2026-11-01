# MN316 软件控制组件

`mn316_ctrl` 是独立 ESP-IDF 组件。`bepio_mn316_manager` 管理蜂窝连接、EMQX 会话、
重连和消息队列；硬件传输由 [`drivers/mn316`](../../drivers/mn316/README.md) 提供。
`system/controller` 只协调系统启动。主工程与 `debug/t_mn316` 使用同一套实现。

## 文件与配置

| 文件 | 职责 |
|---|---|
| `bepio_mn316_manager.c/.h` | 对外 API、后台连接、消息领取/PUBACK 完成、收件去重入箱 |
| `bepio_mn316_session.c/.h` | MQTT CONNECT/SUBSCRIBE、包分段、字节流适配、有界诊断会话 |
| `mn316_mqtt.c/.h` | 主工程 QoS 1 收发、PUBACK 匹配、确认超时、心跳 |
| `pager_message.c/.h` | 五字段 JSON 编解码、日期/位置/长度检查 |
| `bepio_mn316_config.h` | Host、8883 端口、本机 ID、主题规则；无密码 |
| `certs/emqx_ca.pem` | 经此前实板验证的 CA；由 CMake 嵌入固件 |
| `emqx_local.h` | 唯一本地 MQTT 用户名/密码配置，已被 Git 忽略 |
| `bepio_mn316_legacy.c`、`onenet.c/.h` | 保留 OneNET 模组内置 MQTT 兼容入口 |

新环境将 `emqx_local.example.h` 复制为同目录 `emqx_local.h`，填入普通 MQTT 账号。
调试目录和 archive 里的同名文件不再被当前代码读取；不要填写管理 API ID/Secret。
此次成功使用用户确认的当前账号，之前失败是沿用了旧账号凭据，而不是 Client ID 拼写错误。
密码仅保留在本地忽略文件，不写入文档。

主固件使用账户中保存的数字设备 ID 作为 MQTT Client ID、JSON From 和收件主题。
首次启动时默认 ID 为 eFuse MAC 的十进制表示。MQTT username 是独立的认证字段；
新增联系人无需填写对方的 MQTT 密码，也不会替对方创建 EMQX 认证账号。

## 收件箱、联系人与规则

每台设备只订阅自己的 inbox，发送时使用收件人的 ID 生成主题：

| 收件人 | 接收方订阅 / 发送方发布 |
|---|---|
| 100000000000001 | `pager/demo/100000000000001/inbox` |
| 100000000000002 | `pager/demo/100000000000002/inbox` |
| 13800138000 | `pager/demo/13800138000/inbox` |

发送使用联系人的数字设备 ID，不使用显示姓名。统一路由为
`pager/demo/{数字ID}/inbox`，JSON 的 `To` 也是该数字 ID。
Contacts 新增/编辑/删除只影响本地通讯录，不产生 MQTT SUBSCRIBE/UNSUBSCRIBE，
也不阻止已删除联系人向本机 inbox 发消息。已排队消息保存收件人快照，删除联系人不会改变它。
服务器需允许本账号订阅本机 inbox、向目标 inbox 发布；服务端认证与 ACL 不会随联系人自动创建。

主工程正文最多 256 UTF-8 字节，联系人设备 ID 最多 24 位且仅含数字。
数字 ID 通过 MQTT 主题路由，接收设备或服务端须订阅对应主题；这不是短信发送。

所有消息区分大小写，使用以下结构；当前无定位来源，发送 `At: null`：

```json
{"From":"100000000000001","To":"100000000000002","Text":"hahaha","Datetime":"2026-11-01T15:47:21Z","At":null}
```

## 主工程的后台行为

`app_main` 先初始化消息存储，再经系统协调器探测模组，并显式调用
`bepio_mn316_manager_emqx_start()`。工作任务等待注册、附着和 IP，完成运营商校时、
蜂窝 DNS/TCP、验证 TLS、MQTT 登录、订阅本机 inbox 后，才将消息服务标为在线。
主工程常驻连接，不使用调试工程的 30 秒退出模式，也不自动发送测试消息或回显下行。

发送队列使用 QoS 1、非保留消息，收到匹配 PUBACK 才标为已发送；30 秒无确认则失败。
每 20 秒发送心跳，15 秒未收到 PINGRESP 则退出重连。普通失败等待 10 秒，认证失败等待 60 秒。
PUBACK 只表示 broker 确认，不能冒充对方实际收件或已读；失败后手动重试可能重复送达。

有效下行先校验 To、字段和长度，再去重、持久化到 Inbox；存储失败不发收件 PUBACK。
当前 clean session 不保证断线后的补投，QoS 0 下行也无可靠重投保证。
旧五字段协议没有独立消息 ID，以规范 JSON 的哈希去重，同秒同内容可能合并。

## 稍后的 Contacts 实板测试

1. `./build.ps1` 构建主工程，再从已激活 ESP-IDF 的终端执行
   `idf.py -p COM8 flash monitor`（换成实际端口）。模型 Flash 仍默认跳过。
   切换独立 debug 工程与主工程时应烧录完整 bootloader/分区表/app，不只 app。
2. 等待 `mn316_manager: EMQX online; Client ID=<本机数字ID> inbox=...`；
   `TLS PASS` 本身不表示 MQTT 登录、订阅或对方收件成功。
3. PC 使用独立数字 Client ID，连接同一部署并**先成功订阅** `pager/demo/100000000000002/inbox`。
4. Contacts 新增姓名 `PC`，设备 ID 填 `100000000000002`。Messages 选择此联系人，输入 `hahaha` 并发送。
5. 核对 PC 实际收到五字段 JSON；设备的“已发送”仅代表 PUBACK。
6. PC 向本机数字 ID 的收件主题发布 To=<本机数字ID> 的有效 JSON，
   在设备 Inbox 确认入箱。新增/删除联系人期间本机订阅仍保持不变。

## 调试与验证范围

`./build.ps1 -Application debug/t_mn316` 构建独立调试固件。
`emqx_send 30 hahaha` 完成有限会话；`emqx_send 600 hahaha` 可在线 10 分钟。
这是明确的诊断回显会话，与主工程队列会话不同。
`python debug/t_mn316/test_emqx.py --port COM8 --device-id <本机数字ID> --text hahaha` 使用额外的临时 PC 订阅者，
其成功只证明测试订阅者收到，不能代替用户实际接收设备的确认。

迁移前已由用户确认 PC 订阅正确主题后实际收到 `hahaha`。
迁移版完成主/调试工程构建，以及网络解析、分段读写、认证拒绝、主题、JSON、
QoS 1、Contacts 到队列等主机回归；尚未重新烧录验证迁移后的 FreeRTOS 行为和主工程 Contacts 收发。
